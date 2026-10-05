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

constexpr float DEFAULT_MIN_P = 0.05f;
constexpr float DEFAULT_TEMP = 0.3f;
constexpr uint16_t DEFAULT_MAX_CTX = 32768;
constexpr char DEFAULT_MODEL[] =
        "/home/beyluta/Downloads/qwen2.5-coder-1.5b-instruct-q4_k_m.gguf";

typedef struct llama_model llama_model_t;
typedef struct llama_vocab llama_vocab_t;
typedef struct llama_context llama_context_t;
typedef struct llama_sampler llama_sampler_t;
typedef struct llama_batch_ext llama_batch_ext_t;
typedef struct llama_chat_message llama_chat_message_t;
typedef struct llama_model_params llama_model_params_t;
typedef struct llama_context_params llama_context_params_t;

struct agent_llama
{
        llama_model_t *model;
        llama_context_t *context;
        llama_sampler_t *sampler;
        llama_batch_ext_t *batch;
        size_t message_count;
};

agent_llama_t *
agent_new_instance()
{
        llama_model_params_t model_params = llama_model_default_params();
        llama_model_t *model =
                llama_model_load_from_file(DEFAULT_MODEL, model_params);
        if (!model)
        {
                fprintf(stderr, "Failed to load GGUF model from file.\n");
                return nullptr;
        }

        llama_context_params_t context_params = llama_context_default_params();
        context_params.n_ctx = DEFAULT_MAX_CTX;
        context_params.n_batch = DEFAULT_MAX_CTX;

        llama_context_t *context = llama_init_from_model(model, context_params);
        if (!context)
        {
                llama_model_free(model);
                return nullptr;
        }

        llama_sampler_chain_params sampler_params =
                llama_sampler_chain_default_params();
        sampler_params.no_perf = true;

        llama_sampler_t *sampler = llama_sampler_chain_init(sampler_params);
        if (!sampler)
        {
                llama_free(context);
                llama_model_free(model);
                return nullptr;
        }

        llama_sampler_chain_add(sampler,
                                llama_sampler_init_min_p(DEFAULT_MIN_P, 1));
        llama_sampler_chain_add(sampler, llama_sampler_init_temp(DEFAULT_TEMP));
        llama_sampler_chain_add(sampler,
                                llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

        llama_batch_ext_t *batch = llama_batch_ext_init(context);

        agent_llama_t *agent_llama =
                (agent_llama_t *)malloc(sizeof(agent_llama_t));
        if (!agent_llama)
        {
                llama_sampler_free(sampler);
                llama_free(context);
                llama_model_free(model);
                return nullptr;
        }

        agent_llama->model = model;
        agent_llama->context = context;
        agent_llama->sampler = sampler;
        agent_llama->batch = batch;
        agent_llama->message_count = 0;

        return agent_llama;
}

void
agent_free_instance(agent_llama_t *restrict const inst)
{
        if (!inst)
        {
                return;
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
agent_append_response(char **const response,
                      const char *restrict const new_response)
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
                return 0;
        }

        char *data = *response;
        memcpy(&data[response_len], new_response, new_response_len);
        data[response_len + new_response_len] = 0;

        return response_len + new_response_len;
}

static char *
agent_send_prompt(agent_llama_t *restrict const agent_llama,
                  const char *restrict const prompt)
{
        llama_memory_t memory = llama_get_memory(agent_llama->context);

        const bool is_first = llama_memory_seq_pos_max(memory, 0) == -1;

        const llama_vocab_t *vocab = llama_model_get_vocab(agent_llama->model);

        const int n_prompt_tokens = -llama_tokenize(
                vocab, prompt, strlen(prompt), nullptr, 0, is_first, true);

        // vector_t *prompt_tokens =
        //         vector_new(sizeof(llama_token), n_prompt_tokens);
        llama_token *prompt_tokens =
                (llama_token *)malloc(sizeof(llama_token) * n_prompt_tokens);
        if (!prompt_tokens)
        {
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
                free(prompt_tokens);
                return nullptr;
        }

        const llama_token *tokens = prompt_tokens;
        int n_tokens = n_prompt_tokens;

        char *response = nullptr;
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

                char *piece = (char *)malloc(sizeof(char) * (n + 1));
                if (!piece)
                {
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

static llama_chat_message_t *
agent_new_message(const char *restrict const user,
                  const char *restrict const content)
{
        if (!user || !content)
        {
                return nullptr;
        }

        char *pUser = (char *)malloc(sizeof(char) * (strlen(user) + 1));
        if (!pUser)
        {
                return nullptr;
        }
        memcpy(pUser, user, strlen(user));
        pUser[strlen(user)] = 0;

        char *pContent = (char *)malloc(sizeof(char) * (strlen(content) + 1));
        if (!pContent)
        {
                free(pUser);
                return nullptr;
        }
        memcpy(pContent, content, strlen(content));
        pContent[strlen(content)] = 0;

        llama_chat_message_t *message =
                (llama_chat_message_t *)malloc(sizeof(llama_chat_message_t));
        if (!message)
        {
                free(pContent);
                free(pUser);
                return nullptr;
        }

        message->role = pUser;
        message->content = pContent;

        return message;
}

static void
agent_free_message(llama_chat_message_t *restrict const message)
{
        if (!message)
        {
                return;
        }

        if (message->content)
        {
                free((char *)message->content);
        }

        if (message->role)
        {
                free((char *)message->role);
        }

        free(message);
}

static void
agent_free_chat_messages(llama_chat_message_t **message, const size_t size)
{
        for (size_t i = 0; i < size; i++)
        {
                agent_free_message(message[i]);
        }

        free(message);
}

static void
agent_message_push_back(llama_chat_message_t ***messages,
                        size_t *restrict messages_size,
                        llama_chat_message_t *restrict const message)
{
        if (!messages || !message)
        {
                return;
        }

        *messages_size = *messages_size + 1;

        llama_chat_message_t **new_messages = (llama_chat_message_t **)malloc(
                sizeof(llama_chat_message_t *) * (*messages_size));
        if (!new_messages)
        {
                return;
        }

        for (size_t i = 0; i < *messages_size - 1; i++)
        {
                new_messages[i] = *messages[i];
        }

        new_messages[*messages_size - 1] = message;

        if (*messages)
        {
                free(*messages);
        }

        *messages = new_messages;
}

char *
agent_generate_prompt(agent_llama_t *restrict const agent_llama,
                      const char *restrict const prompt)
{
        size_t formatted_size = llama_n_ctx(agent_llama->context);
        char *formatted = (char *)malloc(sizeof(char) * (formatted_size + 1));
        if (!formatted)
        {
                return nullptr;
        }

        const char *tmpl =
                llama_model_chat_template(agent_llama->model, nullptr);

        llama_chat_message_t *new_message = agent_new_message("user", prompt);
        if (!new_message)
        {
                free(formatted);
                return nullptr;
        }

        size_t messages_size = 0;
        llama_chat_message_t **messages = nullptr;
        agent_message_push_back(&messages, &messages_size, new_message);

        int new_len = llama_chat_apply_template(tmpl,
                                                *messages,
                                                messages_size,
                                                true,
                                                formatted,
                                                formatted_size);
        if (new_len > (int)formatted_size)
        {
                formatted = realloc(formatted, sizeof(char) * (new_len + 1));
                formatted_size = new_len;
                new_len = llama_chat_apply_template(tmpl,
                                                    *messages,
                                                    messages_size,
                                                    true,
                                                    formatted,
                                                    formatted_size);
        }

        if (new_len < 0)
        {
                fprintf(stderr, "Failed to apply chat template\n");
                agent_free_chat_messages(messages, messages_size);
                free(formatted);
                return nullptr;
        }

        char *response = agent_send_prompt(agent_llama, formatted);
        if (!response)
        {
                fprintf(stderr, "Send prompt for processing\n");
                agent_free_chat_messages(messages, messages_size);
                free(formatted);
                return nullptr;
        }

        agent_free_chat_messages(messages, messages_size);
        free(formatted);

        return response;
}
