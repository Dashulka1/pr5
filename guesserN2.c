#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

#define FIFO_GUESS "/tmp/guess_fifo"
#define FIFO_RESULT "/tmp/result_fifo"
#define MIN_NUM 1
#define ROUNDS 10

void cleanup() {
    unlink(FIFO_GUESS);
    unlink(FIFO_RESULT);
}

void setter(int max_number) {
    srand(getpid() ^ time(NULL));
    int guess_fd = open(FIFO_GUESS, O_RDONLY);
    int result_fd = open(FIFO_RESULT, O_WRONLY);

    if (guess_fd < 0 || result_fd < 0) {
        perror("setter open");
        exit(EXIT_FAILURE);
    }

    for (int round = 1; round <= ROUNDS; ++round) {
        int secret = MIN_NUM + rand() % max_number;
        printf("[setter %d] Загадал число от 1 до %d\n", getpid(), max_number);

        int guess;
        while (1) {
            if (read(guess_fd, &guess, sizeof(int)) != sizeof(int)) {
                perror("setter read");
                break;
            }

            if (guess == secret) {
                int result = 1;
                write(result_fd, &result, sizeof(int));
                break;
            } else {
                int result = 0;
                write(result_fd, &result, sizeof(int));
            }
        }
    }

    close(guess_fd);
    close(result_fd);
}

void guesser(int max_number) {
    srand(getpid() ^ time(NULL));
    int guess_fd = open(FIFO_GUESS, O_WRONLY);
    int result_fd = open(FIFO_RESULT, O_RDONLY);

    if (guess_fd < 0 || result_fd < 0) {
        perror("guesser open");
        exit(EXIT_FAILURE);
    }

    for (int round = 1; round <= ROUNDS; ++round) {
        int attempts = 0;
        struct timespec start, end;
        clock_gettime(CLOCK_MONOTONIC, &start);

        while (1) {
            int guess = MIN_NUM + rand() % max_number;
            if (write(guess_fd, &guess, sizeof(int)) != sizeof(int)) {
                perror("guesser write");
                break;
            }

            attempts++;

            int result;
            if (read(result_fd, &result, sizeof(int)) != sizeof(int)) {
                perror("guesser read");
                break;
            }

            if (result == 1) {
                clock_gettime(CLOCK_MONOTONIC, &end);
                double time_spent = (end.tv_sec - start.tv_sec) +
                                    (end.tv_nsec - start.tv_nsec) / 1e9;
                printf("[guesser %d] Угадал число %d за %d попыток (время: %.2fs)\n",
                       getpid(), guess, attempts, time_spent);
                break;
            }
        }
    }

    close(guess_fd);
    close(result_fd);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <макс_число>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int max_number = atoi(argv[1]);
    if (max_number < 1) {
        fprintf(stderr, "Максимальное число должно быть > 0\n");
        exit(EXIT_FAILURE);
    }

    atexit(cleanup); // удалим FIFO после завершения

    if (mkfifo(FIFO_GUESS, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo guess");
        exit(EXIT_FAILURE);
    }
    if (mkfifo(FIFO_RESULT, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo result");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        // Родитель — setter
        setter(max_number);
        wait(NULL);
    } else {
        // Ребенок — guesser
        guesser(max_number);
    }

    return 0;
}
