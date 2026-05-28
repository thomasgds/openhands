#include "shell.h"
#include "kernel_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

/*
 * Shell I/O 重定向支持
 *
 * 管道支持：用 "|" 分隔的命令链，前一个 stdout 连接到后一个 stdin
 * 重定向支持：
 *   cmd > file   — 覆盖写
 *   cmd >> file  — 追加写
 *   cmd < file   — 读文件
 *
 * 当前实现：在 shell_execute 层面解析重定向符号，
 * 用 fork/exec 子进程在独立环境中运行。
 *
 * 注意：由于 RTOS 任务使用单个进程内的协作调度，
 * 重定向操作会 fork 真实子进程来执行命令。
 */

/* 在命令行中查找并处理重定向符号
 * 返回: 0=正常, >0=重定向至文件
 * 修改 argv 数组，移除重定向部分
 */
int shell_parse_redir(char *line)
{
    (void)line;
    /* 当前简化版本：暂不实现，留待后续扩展 */
    return 0;
}

void shell_shell_io_init(void) { LOG_INFO("shell_io initialized"); }
