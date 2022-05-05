#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>

#define ROOT 0

int main(int argc, char **argv)
{

    srand(time(NULL));
    int size, rank;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    MPI_Status status;
    int res;
    int i;

    // indeks to rank procesu, wartosc to wartosc timestampu tego procesu,
    // a miejce w kolejce to ilosc procesow z mniejszym timestampem + 1
    int timestamps[size - 1];
    memset(timestamps, 0, sizeof timestamps);

    int ok = -1;
    int no = -2;

    int free[size];
    for (i = 0; i < size; i++)
        free[i] = 1;
    int im_free = -3;
    int im_busy = -4;
    int free_size = size - 1 - 1;

    int cs_size = 0;

    int ok_num = 0; // ilosc otrzymanych ok
    int got_no = 0; // czy dostalem no

    int timestamp = 0;
    int req_timestamp = 0;

    void *listening(void *arg)
    {
        while (1)
        {
            printf("Jestem %d 1, czekam na wiadomość -> %d\n", rank, timestamp);
            MPI_Recv(&res, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            printf("Jestem %d 1, dostałem wiadomość %d od %d -> %d\n", rank, res, status.MPI_SOURCE, timestamp);
            sleep(1);
            if (status.MPI_SOURCE != 0)
            { // otrzymano timestamp
                timestamps[status.MPI_SOURCE - 1] = res;
                if (res == ok)
                {
                    ok_num++;
                    printf("Jestem %d 1, zwiększam licznik OK z %d do %d -> %d\n", rank, ok_num - 1, ok_num, timestamp);
                }
                else if (res == no)
                {
                    got_no = 1;
                    ok_num = 0;
                    printf("Jestem %d 1, ustawiam, że dostałem NO -> %d\n", rank, timestamp);
                }
                else if (res == im_free)
                {
                    free[status.MPI_SOURCE] = 1;
                    free_size++;
                    printf("Jestem %d 1, ustawiam, że %d jest wolny -> %d\n", rank, status.MPI_SOURCE, timestamp);
                }
                else if (res == im_busy)
                {
                    free[status.MPI_SOURCE] = 0;
                    free_size--;
                    printf("Jestem %d 1, ustawiam, że %d jest zajęty -> %d\n", rank, status.MPI_SOURCE, timestamp);
                }
                else if (res > req_timestamp || (res == req_timestamp && rank < status.MPI_SOURCE)) // ja mam wczesniejszy timestamp
                {
                    printf("Jestem %d 1, wysyłam NO do %d -> %d\n", rank, status.MPI_SOURCE, timestamp);
                    timestamp = res;

                    sleep(1);
                    timestamp++;
                    MPI_Send(&no, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
                }
                else if (res < req_timestamp || (res == req_timestamp && rank > status.MPI_SOURCE)) // ja mam pozniejszy timestamp
                {
                    printf("Jestem %d 1, wysyłam OK do %d -> %d\n", rank, status.MPI_SOURCE, timestamp);

                    sleep(1);
                    timestamp++;
                    MPI_Send(&ok, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD);
                }
            }
            else
            {
                printf("Jestem %d 1, zwiększam sobie rozmiar ścieżki krytycznej -> %d\n", rank, timestamp);
                cs_size++;
            }
        }
    }

    if (rank == ROOT)
    { // instytut
        sleep(1);
        int time_sleep = 15;
        int order_id;
        while (1)
        {
            printf("Jestem instytut i wysyłam do wszystkich ogrodników info, że jest nowe zlecenie!\n");
            order_id = (rand() % (10000 - 5000 + 1)) + 5000; // id zlecen miedzy 5000, a 10000, by latwo rozroznic z timestampami
            for (i = 1; i < size; i++)
            {
                MPI_Send(&order_id, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            }
            for (i = time_sleep; i > 0; i--)
            {
                printf("%d\n", i);
                sleep(1);
            }
        }
    }
    else
    {
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, listening, NULL);
        while (1)
        {
            if (cs_size > 0)
            {
                timestamp++;
                req_timestamp++;
                for (i = 1; i < size; i++)
                {
                    if (i != rank && free[i] == 1)
                    {
                        printf("Jestem %d 2, wysyłam prośbę o OK do %d -> %d\n", rank, i, req_timestamp);

                        sleep(1);

                        MPI_Send(&req_timestamp, 1, MPI_INT, i, 0, MPI_COMM_WORLD); // pytam o ok
                    }
                }
                while (1)
                {
                    if (got_no == 1)
                    {
                        printf("Jestem %d 2, otrzymałem NO od %d -> %d\n", rank, status.MPI_SOURCE, timestamp);
                        cs_size--;
                        ok_num = 0;
                        got_no = 0;
                        break;
                    }
                    if (ok_num == free_size)
                    {
                        printf("Jestem %d 2, otrzymałem OK od wszsytkich, wchodzę do sekcji krytycznej -> %d\n", rank, timestamp);
                        for (i = 1; i < size; i++)
                        {
                            if (i != rank)
                            {
                                MPI_Send(&im_busy, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                            }
                        }
                        for (i = 50; i > 0; i--)
                        {
                            printf("Jestem %d 2, jestem w sekcji krytycznej przez jeszcze: %d -> %d\n", rank, i, timestamp);
                            sleep(1);
                        }
                        printf("Jestem %d 2, wychodzę z sekcji krytycznej -> %d\n", rank, timestamp);
                        cs_size--;
                        ok_num = 0;
                        break;
                    }
                }
            }
        }
    }
    MPI_Finalize();
}
