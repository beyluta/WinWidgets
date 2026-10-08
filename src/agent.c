/**
 * WinWidgets is a widget application for Windows 11 and Linux.
 * Copyright (c) 2026 Pedro Ribeiro.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "agent.h"
#include "llama.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uchar.h>

// Private members

typedef struct llama_model llama_model_t;
typedef struct llama_vocab llama_vocab_t;
typedef struct llama_context llama_context_t;
typedef struct llama_sampler llama_sampler_t;
typedef struct llama_batch_ext llama_batch_ext_t;
typedef struct llama_model_params llama_model_params_t;
typedef struct llama_context_params llama_context_params_t;

struct agent_llama
{
        llama_model_t *model;
        llama_context_t *context;
        llama_sampler_t *sampler;
        llama_batch_ext_t *batch;
        llama_chat_message *messages;
        size_t message_size;
        int prev_len;
};

static void
batch_set_tokens(llama_batch_ext_t *batch,
                 const llama_token *tokens,
                 int32_t n_tokens,
                 llama_pos pos_0)
{
        llama_batch_ext_clear(batch);
        for (int32_t i = 0; i < n_tokens; ++i)
        {
                const int32_t idx =
                        llama_batch_ext_add_token(batch, 0, tokens[i]);
                const llama_pos pos = pos_0 + i;
                llama_batch_ext_set_pos(batch, idx, &pos);
        }
        llama_batch_ext_set_output_logits(batch, n_tokens - 1, true);
}

static size_t
agent_append_response(string *const response, const string new_response)
{
        size_t response_len = 0;
        if (*response)
        {
                response_len = strlen(*response);
        }

        const size_t new_response_len = strlen(new_response);

        *response =
                realloc(*response,
                        sizeof(char) * (response_len + new_response_len + 1));
        if (!*response)
        {
                fprintf(stderr,
                        "Failed to reallocate memory for new response\n");
                return 0;
        }

        string data = *response;
        memcpy(&data[response_len], new_response, new_response_len);
        data[response_len + new_response_len] = 0;

        return response_len + new_response_len;
}

static void
agent_free_message(llama_chat_message message)
{
        if (message.content)
        {
                free((string)message.content);
        }

        if (message.role)
        {
                free((string)message.role);
        }
}

static string
agent_send_prompt(agent_llama_t *restrict const agent_llama,
                  const string prompt)
{
        llama_memory_t memory = llama_get_memory(agent_llama->context);

        const bool is_first = llama_memory_seq_pos_max(memory, 0) == -1;

        const llama_vocab_t *vocab = llama_model_get_vocab(agent_llama->model);

        const int n_prompt_tokens = -llama_tokenize(
                vocab, prompt, strlen(prompt), nullptr, 0, is_first, true);

        llama_token *prompt_tokens =
                (llama_token *)malloc(sizeof(llama_token) * n_prompt_tokens);
        if (!prompt_tokens)
        {
                fprintf(stderr,
                        "Failed to reallocate memory for prompt tokens\n");
                return nullptr;
        }

        if (llama_tokenize(vocab,
                           prompt,
                           strlen(prompt),
                           prompt_tokens,
                           n_prompt_tokens,
                           is_first,
                           true) < 0)
        {
                fprintf(stderr, "Failed to tokenize prompt\n");
                free(prompt_tokens);
                return nullptr;
        }

        const llama_token *tokens = prompt_tokens;
        int n_tokens = n_prompt_tokens;

        string response = nullptr;
        llama_token new_toked_id;
        while (true)
        {
                int n_ctx = llama_n_ctx(agent_llama->context);
                int n_ctx_used =
                        llama_memory_seq_pos_max(
                                llama_get_memory(agent_llama->context), 0) +
                        1;
                if (n_ctx_used + n_tokens > n_ctx)
                {
                        fprintf(stderr, "Context window has been reached\n");
                        free(prompt_tokens);
                        return nullptr;
                }

                batch_set_tokens(
                        agent_llama->batch, tokens, n_tokens, n_ctx_used);

                const int ret = llama_process(agent_llama->context,
                                              LLAMA_PROCESS_TYPE_DECODE,
                                              agent_llama->batch);
                if (ret != 0)
                {
                        fprintf(stderr, "Failed to decode\n");
                        free(prompt_tokens);
                        return nullptr;
                }

                new_toked_id = llama_sampler_sample(
                        agent_llama->sampler, agent_llama->context, -1);

                if (llama_vocab_is_eog(vocab, new_toked_id))
                {
                        break;
                }

                char buf[256];
                int n = llama_token_to_piece(
                        vocab, new_toked_id, buf, sizeof(buf), 0, true);
                if (n < 0)
                {
                        fprintf(stderr, "Failed to convert token to piece\n");
                        free(prompt_tokens);
                        return nullptr;
                }

                string piece = (string)malloc(sizeof(char) * (n + 1));
                if (!piece)
                {
                        fprintf(stderr, "Failed allocate memory for piece\n");
                        free(prompt_tokens);
                        return nullptr;
                }

                memcpy(piece, buf, n);
                piece[n] = 0;

                fflush(stdout);

                const size_t bytes = agent_append_response(&response, piece);
                if (bytes == 0)
                {
                        free(piece);
                        free(prompt_tokens);
                        fprintf(stderr,
                                "Failed to append piece to response string\n");
                        return nullptr;
                }

                tokens = &new_toked_id;
                n_tokens = 1;
                free(piece);
        }

        free(prompt_tokens);
        return response;
}

static llama_chat_message
agent_new_message(const string role, const string content)
{
        llama_chat_message message = {.role = nullptr, .content = nullptr};

        if (!role || !content)
        {
                fprintf(stderr, "Role or Content must not be null\n");
                return message;
        }

        string ptrRole = strdup(role);
        if (!ptrRole)
        {
                fprintf(stderr, "Failed to allocate memory for role\n");
                return message;
        }

        string ptrContent = strdup(content);
        if (!ptrContent)
        {
                fprintf(stderr, "Failed to allocate memory for content\n");
                free(ptrRole);
                return message;
        }

        message.role = ptrRole;
        message.content = ptrContent;

        return message;
}

// Public members

agent_llama_t *
agent_new_instance(agent_llama_options_t options)
{
        llama_model_params_t model_params = llama_model_default_params();
        llama_model_t *model =
                llama_model_load_from_file(options.model_path, model_params);
        if (!model)
        {
                fprintf(stderr, "Failed to load GGUF model from file.\n");
                return nullptr;
        }

        llama_context_params_t context_params = llama_context_default_params();
        context_params.n_ctx = options.max_ctx_size;
        context_params.n_batch = options.max_ctx_size;

        llama_context_t *context = llama_init_from_model(model, context_params);
        if (!context)
        {
                fprintf(stderr, "Failed to initialize context\n");
                llama_model_free(model);
                return nullptr;
        }

        llama_sampler_chain_params sampler_params =
                llama_sampler_chain_default_params();
        sampler_params.no_perf = true;

        llama_sampler_t *sampler = llama_sampler_chain_init(sampler_params);
        if (!sampler)
        {
                fprintf(stderr, "Failed to initialize sampler\n");
                llama_free(context);
                llama_model_free(model);
                return nullptr;
        }

        llama_sampler_chain_add(sampler,
                                llama_sampler_init_min_p(options.min_p, 1));
        llama_sampler_chain_add(sampler, llama_sampler_init_temp(options.temp));
        llama_sampler_chain_add(sampler,
                                llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

        llama_batch_ext_t *batch = llama_batch_ext_init(context);

        agent_llama_t *agent_llama =
                (agent_llama_t *)malloc(sizeof(agent_llama_t));
        if (!agent_llama)
        {
                fprintf(stderr, "Failed to allocate memory for agent\n");
                llama_sampler_free(sampler);
                llama_free(context);
                llama_model_free(model);
                return nullptr;
        }

        agent_llama->model = model;
        agent_llama->context = context;
        agent_llama->sampler = sampler;
        agent_llama->batch = batch;
        agent_llama->message_size = 0;
        agent_llama->prev_len = 0;
        agent_llama->messages = nullptr;

        return agent_llama;
}

void
agent_free_instance(agent_llama_t *restrict const inst)
{
        if (!inst)
        {
                fprintf(stderr, "No llama instance to free\n");
                return;
        }

        if (inst->messages)
        {
                for (size_t i = 0; i < inst->message_size; i++)
                {
                        agent_free_message(inst->messages[i]);
                }

                free(inst->messages);
        }

        if (inst->batch)
        {
                llama_batch_ext_free(inst->batch);
        }

        if (inst->sampler)
        {
                llama_sampler_free(inst->sampler);
        }

        if (inst->context)
        {
                llama_free(inst->context);
        }

        if (inst->model)
        {
                llama_model_free(inst->model);
        }

        free(inst);
}

static size_t
agent_message_push_back(llama_chat_message **messages,
                        size_t *restrict messages_size,
                        llama_chat_message message)
{
        if (!messages || !messages_size || !message.role || !message.content)
        {
                return 0;
        }

        *messages_size = *messages_size + 1;

        *messages = realloc(*messages,
                            sizeof(llama_chat_message) * (*messages_size));
        if (!messages)
        {
                return 0;
        }

        llama_chat_message *messagesPtr = (llama_chat_message *)*messages;
        memcpy(&messagesPtr[*messages_size - 1],
               &message,
               sizeof(llama_chat_message));

        return *messages_size;
}

string
agent_generate_prompt(agent_llama_t *restrict const agent_llama,
                      const string prompt)
{
        size_t formatted_size = llama_n_ctx(agent_llama->context);
        string formatted = (string)malloc(sizeof(char) * (formatted_size + 1));
        if (!formatted)
        {
                fprintf(stderr,
                        "Failed to allocate memory for formatted string\n");
                return nullptr;
        }

        const string tmpl = (const string)llama_model_chat_template(
                agent_llama->model, nullptr);

        llama_chat_message user_message = agent_new_message("user", prompt);
        if (!user_message.role || !user_message.content)
        {
                fprintf(stderr, "Failed to create new chat message\n");
                free(formatted);
                return nullptr;
        }

        if (agent_message_push_back(&agent_llama->messages,
                                    &agent_llama->message_size,
                                    user_message) == 0)
        {
                fprintf(stderr, "Failed to add user message to messages\n");
                agent_free_message(user_message);
                free(formatted);
                return nullptr;
        }

        int new_len = llama_chat_apply_template(tmpl,
                                                agent_llama->messages,
                                                agent_llama->message_size,
                                                true,
                                                formatted,
                                                formatted_size);
        if (new_len > (int)formatted_size)
        {
                formatted = realloc(formatted, sizeof(char) * (new_len + 1));
                formatted_size = new_len;
                new_len = llama_chat_apply_template(tmpl,
                                                    agent_llama->messages,
                                                    agent_llama->message_size,
                                                    true,
                                                    formatted,
                                                    formatted_size);
        }

        if (new_len < 0)
        {
                fprintf(stderr, "Failed to apply chat template\n");
                agent_free_message(user_message);
                free(formatted);
                return nullptr;
        }

        string new_prompt = AllocSubstr(formatted,
                                        formatted_size,
                                        agent_llama->prev_len,
                                        new_len - agent_llama->prev_len);
        if (!new_prompt)
        {
                fprintf(stderr, "Failed to apply chat template\n");
                agent_free_message(user_message);
                free(formatted);
        }

        string response = agent_send_prompt(agent_llama, new_prompt);
        if (!response)
        {
                fprintf(stderr, "Failed sending prompt for processing\n");
                free(new_prompt);
                agent_free_message(user_message);
                free(formatted);
                return nullptr;
        }

        llama_chat_message assistant_message =
                agent_new_message("assistant", response);
        if (!assistant_message.role || !assistant_message.content)
        {
                fprintf(stderr, "Failed to create prompt for assistant\n");
                free(new_prompt);
                agent_free_message(user_message);
                free(formatted);
                return nullptr;
        }

        if (agent_message_push_back(&agent_llama->messages,
                                    &agent_llama->message_size,
                                    assistant_message) == 0)
        {
                fprintf(stderr, "Failed to create prompt for assistant\n");
                agent_free_message(assistant_message);
                free(new_prompt);
                agent_free_message(user_message);
                free(formatted);
                return nullptr;
        }

        agent_llama->prev_len =
                llama_chat_apply_template(tmpl,
                                          agent_llama->messages,
                                          agent_llama->message_size,
                                          false,
                                          nullptr,
                                          0);
        if (agent_llama->prev_len < 0)
        {
                fprintf(stderr, "Failed to apply template for assistant\n");
                agent_free_message(assistant_message);
                free(new_prompt);
                agent_free_message(user_message);
                free(formatted);
                return nullptr;
        }

        free(new_prompt);
        free(formatted);

        return response;
}

void
agent_append_system_instruction(agent_llama_t *agent_llama, const string prompt)
{
        llama_chat_message message = agent_new_message("system", prompt);
        if (!message.role || !message.content)
        {
                fprintf(stderr,
                        "Failed to create new system instruction message\n");
                return;
        }

        if (agent_message_push_back(&agent_llama->messages,
                                    &agent_llama->message_size,
                                    message) == 0)
        {
                fprintf(stderr, "Failed to add system instruction\n");
                return;
        }
}

string
agent_tool_strip_codeblock_response(const string src)
{
        if (!src)
        {
                fprintf(stderr, "Source cannot be empty for strip\n");
                return nullptr;
        }

        const size_t size = strlen(src);
        for (size_t i = 0; i < size; i++)
        {

                if (i + 2 < size && i > 0 && src[i] == '`' &&
                    src[i + 1] == '`' && src[i + 2] == '`')
                {
                        const size_t max = i - 1;

                        if (max >= size)
                        {
                                fprintf(stderr,
                                        "Cannot copy more bytes than "
                                        "allocated\n");
                                return nullptr;
                        }

                        string data = (string)malloc(sizeof(char) * (max + 1));
                        if (!data)
                        {
                                fprintf(stderr,
                                        "Failed to allocate memory for "
                                        "stripped message without codeblock\n");
                                return nullptr;
                        }
                        memcpy(data, src, max);
                        data[max] = 0;
                        return data;
                }
        }

        return nullptr;
}

string
agent_tool_codeblock_scan(const string src)
{
        if (!src)
        {
                fprintf(stderr, "Prompt source cannot be empty for scan\n");
                return nullptr;
        }

        string data = nullptr;
        const size_t max = strlen(src);
        size_t offset = 0;

        for (size_t i = 0; i < max; i++)
        {
                if (offset == 0 && i + 1 < max && i > 1 && src[i] == '`' &&
                    src[i - 1] == '`' && src[i - 2] == '`')
                {
                        offset = i + 1;
                }

                if (offset > 0 && i - 1 > 0 && i + 2 < max && src[i] == '`' &&
                    src[i + 1] == '`' && src[i + 2] == '`')
                {
                        const size_t size = (i - 1) - offset;
                        if (offset >= max || offset + size >= max)
                        {
                                fprintf(stderr,
                                        "Cannot read or write outside of "
                                        "bounds of the memory region\n");
                                return nullptr;
                        }

                        data = malloc(sizeof(char) * (size + 1));
                        if (!data)
                        {
                                fprintf(stderr,
                                        "Failed to allocate memory for "
                                        "resulting codeblock string\n");
                                return nullptr;
                        }

                        memcpy(data, &src[offset], size);
                        data[size] = 0;
                        break;
                }
        }

        return data;
}
