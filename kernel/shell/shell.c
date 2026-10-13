#include "shell.h"
#include "drivers/terminal.h"
#include "drivers/keyboard.h"
#include "process/process.h"
#include "process/scheduler.h"
#include "memory/heap.h"
#include "interrupts/idt.h"
#include <stdint.h>

extern volatile uint64_t timer_ticks;

#define COMMAND_SIZE 64

static char command[COMMAND_SIZE];

static void print_help(void)
{
    terminal_write("\nAvailable commands:\n");
    terminal_write("help     - Show commands\n");
    terminal_write("clear    - Clear screen\n");
    terminal_write("status   - Show kernel status\n");
    terminal_write("ticks    - Show timer ticks\n");
    terminal_write("process  - Show process count\n");
    terminal_write("scheduler- Show scheduling algorithm\n");
}

static void execute_command(void)
{
    if (command[0] == '\0')
    {
        return;
    }

    if (command[0] == 'h' && command[1] == 'e' &&
        command[2] == 'l' && command[3] == 'p' &&
        command[4] == '\0')
    {
        print_help();
    }
    else if (command[0] == 'c' && command[1] == 'l' &&
             command[2] == 'e' && command[3] == 'a' &&
             command[4] == 'r' && command[5] == '\0')
    {
        terminal_clear();
    }
    else if (command[0] == 's' && command[1] == 't' &&
             command[2] == 'a' && command[3] == 't' &&
             command[4] == 'u' && command[5] == 's' &&
             command[6] == '\0')
    {
        terminal_write("\nJARVIS-OS: ONLINE\n");
        terminal_write("Architecture: x86-64\n");
    }
    else if (command[0] == 't' && command[1] == 'i' &&
             command[2] == 'c' && command[3] == 'k' &&
             command[4] == 's' && command[5] == '\0')
    {
        terminal_write("\nTimer ticks: ");
        terminal_write_number((uint32_t)timer_ticks);
        terminal_putchar('\n');
    }
    else if (command[0] == 'p' && command[1] == 'r' &&
             command[2] == 'o' && command[3] == 'c' &&
             command[4] == 'e' && command[5] == 's' &&
             command[6] == 's' && command[7] == '\0')
    {
        terminal_write("\nProcess count: ");
        terminal_write_number(process_get_count());
        terminal_putchar('\n');
    }
    else if (command[0] == 's' && command[1] == 'c' &&
             command[2] == 'h' && command[3] == 'e' &&
             command[4] == 'd' && command[5] == 'u' &&
             command[6] == 'l' && command[7] == 'e' &&
             command[8] == 'r' && command[9] == '\0')
    {
        scheduler_algorithm_t algorithm = scheduler_get_algorithm();

        terminal_write("\nScheduler: ");

        if (algorithm == SCHEDULER_FCFS)
        {
            terminal_write("FCFS\n");
        }
        else if (algorithm == SCHEDULER_SJF)
        {
            terminal_write("SJF\n");
        }
        else
        {
            terminal_write("Round Robin\n");
        }
    }
    else
    {
        terminal_write("\nUnknown command. Type help.\n");
    }
}

void shell_run(void)
{
    terminal_write("\nJARVIS Shell\n");
    terminal_write("Type help for commands.\n");

    while (1)
    {
        terminal_write("\njarvis> ");

        uint32_t index = 0;

        while (1)
        {
            char c = keyboard_read_char();

            if (c == '\n')
            {
                terminal_putchar('\n');
                command[index] = '\0';
                break;
            }

            if (c == '\b')
            {
                if (index > 0)
                {
                    index--;
                    terminal_putchar('\b');
                }
                continue;
            }

            if (c >= 32 && c <= 126 && index < COMMAND_SIZE - 1)
            {
                command[index++] = c;
                terminal_putchar(c);
            }
        }

        execute_command();
    }
}
