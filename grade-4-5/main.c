#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <time.h>

char *sempahore_name = "/garden-semaphore";
const char *shared_object = "/posix-shared";
int main_shmid;

int columns;
int rows;
const int MAX_OF_SEMAPHORES = 1024;

struct Task
{
    int gardener_id;
    int plot_i;
    int plot_j;
    int working_time;
};

void handlePlot(sem_t **semaphores, int *field, int big_columns, struct Task task)
{
    sem_wait(semaphores[task.plot_i / 2 * big_columns + task.plot_j / 2]);
    printf("Gardener %d takes (row: %d, col: %d)\n", task.gardener_id, task.plot_i, task.plot_j);
    fflush(stdout);
    if (field[task.plot_i * columns + task.plot_j] == 0)
    {
        field[task.plot_i * columns + task.plot_j] = task.gardener_id;
        usleep(task.working_time * 1000);
    }
    else
    {
        usleep(task.working_time / 2 * 1000);
    }
    sem_post(semaphores[task.plot_i / 2 * big_columns + task.plot_j / 2]);
}

void getSemaphores(sem_t **semaphores, int columns, int rows)
{
    for (int k = 0; k < columns * rows / 4; ++k)
    {
        char sem_name[200];
        sprintf(sem_name, "%s%d", sempahore_name, k);

        if ((semaphores[k] = sem_open(sem_name, 0)) == 0)
        {
            perror("sem_open: Can't open semaphore");
            exit(-1);
        };
    }
}

void getField(int **field, int field_size, int *shmid)
{
    if ((*shmid = shm_open(shared_object, O_RDWR | O_NONBLOCK, 0666)) == -1)
    {
        perror("Can't connect to shared memory");
        exit(-1);
    }
    if ((*field = mmap(0, field_size * sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED, *shmid, 0)) < 0)
    {
        printf("Can't connect to shared memory\n");
        exit(-1);
    };
}

void runFirst(int columns, int rows, int workingTimeMilliseconds)
{
    int big_columns = columns / 2;
    int field_size = columns * rows;
    int *field;
    int shmid;

    sem_t *semaphores[MAX_OF_SEMAPHORES];
    getSemaphores(semaphores, columns, rows);

    getField(&field, field_size, &shmid);
    printf("Gardener 1 open memory with field\n");
    fflush(stdout);

    int i = 0;
    int j = 0;
    struct Task task;
    task.gardener_id = 1;
    task.working_time = workingTimeMilliseconds;
    while (i < rows)
    {
        while (j < columns)
        {
            task.plot_i = i;
            task.plot_j = j;
            handlePlot(semaphores, field, big_columns, task);
            ++j;
        }

        ++i;
        --j;

        while (j >= 0)
        {
            task.plot_i = i;
            task.plot_j = j;
            handlePlot(semaphores, field, big_columns, task);
            --j;
        }

        ++i;
        ++j;
    }
    printf("Gardener 1 finish work\n");
    exit(0);
}

void runSecond(int columns, int rows, int workingTimeMilliseconds)
{
    int big_columns = columns / 2;
    int field_size = columns * rows;
    int *field;
    int shmid;

    sem_t *semaphores[MAX_OF_SEMAPHORES];
    getSemaphores(semaphores, columns, rows);

    getField(&field, field_size, &shmid);
    printf("Gardener 2 open memory with field\n");
    fflush(stdout);

    int i = rows - 1;
    int j = columns - 1;

    struct Task task;
    task.gardener_id = 2;
    task.working_time = workingTimeMilliseconds;
    while (j >= 0)
    {
        while (i >= 0)
        {
            task.plot_i = i;
            task.plot_j = j;
            handlePlot(semaphores, field, big_columns, task);
            --i;
        }

        --j;
        ++i;

        while (i < rows)
        {
            task.plot_i = i;
            task.plot_j = j;
            handlePlot(semaphores, field, big_columns, task);
            ++i;
        }

        --i;
        --j;
    }
    printf("Gardener 2 finish work\n");
    exit(0);
}

void unlink_all_semaphores_with_close(int columns, int rows)
{
    for (int k = 0; k < columns * rows / 4; ++k)
    {
        char sem_name[200];
        sprintf(sem_name, "%s%d", sempahore_name, k);

        sem_close(sem_open(sem_name, 0));
        if (sem_unlink(sem_name) < 0)
        {
            perror("Can't unlink semaphore");
            exit(-1);
        };
    }
}

void printField(int *field)
{
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < columns; ++j)
        {
            if (field[i * columns + j] < 0)
            {
                printf("X ");
            }
            else
            {
                printf("%d ", field[i * columns + j]);
            }
        }
        printf("\n");
    }
}

void initializeField(int *field)
{
    for (int i = 0; i < rows; ++i)
    {
        for (int j = 0; j < columns; ++j)
        {
            field[i * columns + j] = 0;
        }
    }

    int percentage = 10 + random() % 21;
    int count_of_bad_plots = columns * rows * percentage / 100;
    for (int i = 0; i < count_of_bad_plots; ++i)
    {
        int row_index;
        int column_index;
        do
        {
            row_index = random() % rows;
            column_index = random() % columns;
        } while (field[row_index * columns + column_index] == -1);

        field[row_index * columns + column_index] = -1;
    }
}

void createSemaphores()
{
    for (int k = 0; k < columns * rows / 4; ++k)
    {
        char sem_name[200];
        sprintf(sem_name, "%s%d", sempahore_name, k);

        sem_t *sem;
        if ((sem = sem_open(sem_name, O_CREAT, 0666, 1)) == 0)
        {
            perror("sem_open: Can't create admin");
            exit(-1);
        };

        int val;
        sem_getvalue(sem, &val);
        if (val != 1)
        {
            printf("Some problems. Please, restart program\n");
            unlink_all_semaphores_with_close(columns, rows);
            shm_unlink(shared_object);
            exit(-1);
        }
    }
}

pid_t chpid1, chpid2;

void keyboard_interruption_handler(int num)
{
    kill(chpid1, SIGINT);
    kill(chpid2, SIGINT);
    printf("Closing\n");
    shm_unlink(shared_object);
    unlink_all_semaphores_with_close(columns, rows);
    exit(0);
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    
    if (argc != 4)
    {
        printf("3 arguments: size, first_speed, second_speed\n");
        exit(-1);
    }
    
    int square_side_size = atoi(argv[1]);
    if (square_side_size * square_side_size > MAX_OF_SEMAPHORES)
    {
        printf("Too big square_side_size\n");
        exit(-1);
    }
    else if (square_side_size < 2)
    {
        printf("Too small square_side_size\n");
        exit(-1);
    }
    
    rows = columns = 2 * square_side_size;
    int field_size = rows * columns;
    int first_gardener_working_time = atoi(argv[2]);
    int second_gardener_working_time = atoi(argv[3]);
    
    if (first_gardener_working_time < 1 || second_gardener_working_time < 1)
    {
        printf("Time should be greater than zero\n");
        exit(-1);
    }
    
    int *field;
    
    if ((main_shmid = shm_open(shared_object, O_CREAT | O_RDWR, 0666)) == -1)
    {
        perror("Can't connect to shared memory");
        exit(-1);
    }
    if (ftruncate(main_shmid, field_size * sizeof(int)) == -1)
    {
        perror("Can't resize shm");
        exit(-1);
    }
    if ((field = mmap(0, field_size * sizeof(int), PROT_WRITE | PROT_READ, MAP_SHARED, main_shmid, 0)) < 0)
    {
        printf("Can't connect to shared memory\n");
        exit(-1);
    };
    printf("Open shared memory\n");
    
    initializeField(field);
    printField(field);
    
    createSemaphores();
    fflush(stdout);
    
    chpid1 = fork();
    if (chpid1 == 0)
    {
        runFirst(columns, rows, first_gardener_working_time);
    }
    else if (chpid1 < 0)
    {
        perror("Can't start first gardener");
        exit(-1);
    }
    
    chpid2 = fork();
    if (chpid2 == 0)
    {
        runSecond(columns, rows, second_gardener_working_time);
    }
    else if (chpid2 < 0)
    {
        perror("Can't start second gardener");
        exit(-1);
    }
    
    signal(SIGINT, keyboard_interruption_handler);
    
    int status = 0;
    waitpid(chpid1, &status, 0);
    waitpid(chpid2, &status, 0);
    
    unlink_all_semaphores_with_close(columns, rows);
    
    printField(field);
    fflush(stdout);
    
    shm_unlink(shared_object);
}
