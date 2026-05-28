#ifndef SHELL_H
#define SHELL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*shell_cmd_func_t)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *help;
    shell_cmd_func_t func;
} shell_cmd_t;

void shell_init(void);
void shell_register_cmd(const shell_cmd_t *cmd);
int shell_execute(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* SHELL_H */
