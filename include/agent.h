#ifndef AGENT_H
#define AGENT_H

#include "utils.h"

typedef struct agent_llama agent_llama_t;

typedef struct agent_llama_options agent_llama_options_t;

struct agent_llama_options
{
        // Absolute path to the .gguf file
        const string model_path;
        // Max content window
        const size_t max_ctx_size;
        // Min token confidence threshold
        const float min_p;
        // Temperature of the model
        const float temp;
};

/**
 * @brief Function create a new agent instance
 * @return The new agent instance for this LLM
 * @note - Programmer must free this instance
 */
agent_llama_t *
agent_new_instance(agent_llama_options_t options);

/**
 * @brief Function to free an instance of an agent
 * @param inst Instance to free
 */
void
agent_free_instance(agent_llama_t *inst);

/**
 * @brief Function to generate a response from the agent
 * @param agent_llama Instance of the running agent
 * @param prompt Prompt to send to the agent
 * @return A heap allocated response string
 * @note - String must be freed by the programmer
 */
char *
agent_generate_prompt(agent_llama_t *agent_llama, const string prompt);

/**
 * @brief Function to append a system instruct
 * @param agent_llama Pointer to the llama agent instance
 * @param prompt System instruction prompt to save
 */
void
agent_append_system_instruction(agent_llama_t *agent_llama,
                                const string prompt);

/**
 * @brief Function to strip the <think> from the result
 * @param src Source containing the think block
 * @return The text after the think block
 * @note - Programmer must free the memory after use
 */
string
agent_tool_strip_think_response(const string src);

/**
 * @brief Function to strip the final codeblock from the response
 * @param src Source containing the codeblock
 * @return The text before the codeblock
 * @note - Programmer must free the memory after use
 */
string
agent_tool_strip_codeblock_response(const string src);

/**
 * @brief Function to scan a string for a codeblock and extract it
 * @param src Source containing the codeblock
 * @return The first instance of any codeblock found
 * @note - Programmer must free the memory after use
 */
string
agent_tool_codeblock_scan(const string src);

#endif
