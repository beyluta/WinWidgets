#ifndef AGENT_H
#define AGENT_H

typedef struct agent_llama agent_llama_t;

/**
 * @brief Function create a new agent instance
 * @return The new agent instance for this LLM
 * @note - Programmer must free this instance
 */
agent_llama_t *
agent_new_instance();

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
agent_generate_prompt(agent_llama_t *agent_llama, const char *prompt);

#endif
