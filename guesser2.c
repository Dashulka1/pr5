#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>  
#include <string.h>
#include <errno.h>

#define MAX_GAMES 10
#define TIMEOUT_SEC 5

volatile sig_atomic_t guess_result = 0;
volatile sig_atomic_t guess_received = 0;
volatile sig_atomic_t guessed_number = 0;

void handle_guess(int sig, siginfo_t *info, void *ucontext) {
    guess_received = 1;
    guessed_number = info->si_value.sival_int;
}

void handle_result(int sig) {
    if (sig == SIGUSR1) {
        guess_result = 1;  // Угадал
    } else if (sig == SIGUSR2) {
        guess_result = 0;  // Не угадал
    }
}

void setup_signal(int sig, void (*handler)(int)) {
    struct sigaction sa = {0};
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(sig, &sa, NULL);
}

void setup_rt_signal(int sig, void (*handler)(int, siginfo_t *, void *)) {
    struct sigaction sa = {0};
    sa.sa_sigaction = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction(sig, &sa, NULL);
}

void player_guesser(pid_t partner_pid, int max_number) {
    srand(time(NULL) ^ getpid());
    int low = 1, high = max_number;
    int attempts = 0;
    time_t start = time(NULL);

    while (1) {
        int guess = (low + high) / 2;
        union sigval value;
        value.sival_int = guess;
        sigqueue(partner_pid, SIGRTMIN, value);

        // Ждем результат
        sigset_t waitset;
        sigemptyset(&waitset);
        sigaddset(&waitset, SIGUSR1);
        sigaddset(&waitset, SIGUSR2);

        struct timespec timeout = {TIMEOUT_SEC, 0};
        int sig = sigtimedwait(&waitset, NULL, &timeout);

        if (sig == -1) {
            perror("sigtimedwait");
            exit(EXIT_FAILURE);
        }

        attempts++;
        if (guess_result) {
            printf("Угадал число %d за %d попыток. Время: %ld сек.\n", guess, attempts, time(NULL) - start);
            break;
        } else {
            if (guess < guessed_number)
                low = guess + 1;
            else
                high = guess - 1;
        }
    }
}

void player_thinker(pid_t partner_pid, int max_number) {
    srand(time(NULL) ^ getpid());
    int secret = rand() % max_number + 1;
    printf("Загадал число: %d\n", secret);

    while (1) {
        pause();  // Ожидание угадывания
        if (!guess_received)
            continue;

        if (guessed_number == secret) {
            kill(partner_pid, SIGUSR1);  // Угадал
            break;
        } else {
            kill(partner_pid, SIGUSR2);  // Не угадал
        }

        guess_received = 0;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Использование: %s <макс_число>\n", argv[0]);
        return 1;
    }

    int max_number = atoi(argv[1]);
    if (max_number < 1) {
        printf("Максимальное число должно быть больше 0.\n");
        return 1;
    }

    sigset_t block_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGRTMIN);
    sigaddset(&block_mask, SIGUSR1);
    sigaddset(&block_mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &block_mask, NULL);

    setup_rt_signal(SIGRTMIN, handle_guess);
    setup_signal(SIGUSR1, handle_result);
    setup_signal(SIGUSR2, handle_result);

    for (int i = 0; i < MAX_GAMES; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            // Дочерний процесс — угадывает
            player_guesser(getppid(), max_number);
            exit(0);
        } else {
            // Родитель — загадывает
            player_thinker(pid, max_number);

            int status;
            waitpid(pid, &status, 0);
        }

        // Меняем роли
        printf("\nМеняем роли...\n");
        sleep(1);
        pid = fork();

        if (pid == 0) {
            player_thinker(getppid(), max_number);
            exit(0);
        } else {
            player_guesser(pid, max_number);
            int status;
            waitpid(pid, &status, 0);
        }

        printf("\nИгра #%d завершена\n\n", i + 1);
    }

    return 0;
}
