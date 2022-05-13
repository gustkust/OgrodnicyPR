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
    int timestamp = 0;

    int req_queue[size - 1];
    memset(req_queue, -1, sizeof req_queue);

    int req_timestamp = 0;

    int req = 1;
    int rel = 0;
    int req_answer = 2;

    int cs_size = 0;
    int cs_size_req = 0;

    int ok_got = 0;
    // int got_no = 0;

    int first;

    void *listening(void *arg)
    {
        while (1)
        {
            MPI_Recv(&res, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            sleep(1);
            if (status.MPI_SOURCE != 0)
            {
                if (status.MPI_TAG == req)
                {
                    if (res > timestamp)
                    {
                        timestamp = res;
                    }

                    timestamp++;
                    MPI_Send(&timestamp, 1, MPI_INT, status.MPI_SOURCE, req_answer, MPI_COMM_WORLD);

                    req_queue[status.MPI_SOURCE - 1] = res;

                    printf("Jestem %d 1, dostalem req od %d, wysylam mu swoj timestamp -> timestamp = %d, ok_got = %d, req_queue = %d %d %d\n", rank, status.MPI_SOURCE, timestamp, ok_got, req_queue[0], req_queue[1], req_queue[2]);
                }
                else if (status.MPI_TAG == rel)
                {

                    if (res > timestamp)
                    {
                        timestamp = res;
                    }

                    req_queue[status.MPI_SOURCE - 1] = -1;

                    printf("Jestem %d 1, dostalem rel od %d -> timestamp = %d, ok_got = %d, req_queue = %d %d %d\n", rank, status.MPI_SOURCE, timestamp, ok_got, req_queue[0], req_queue[1], req_queue[2]);
                }
                else if (status.MPI_TAG = req_answer)
                {

                    if (res > req_timestamp)
                    {
                        ok_got++;
                        if (res > timestamp)
                        {
                            timestamp = res;
                        }
                        printf("Jestem %d 1, dostalem req_answer od %d, zwiekszam ok_got -> timestamp = %d, ok_got = %d, req_queue = %d %d %d\n", rank, status.MPI_SOURCE, timestamp, ok_got, req_queue[0], req_queue[1], req_queue[2]);
                    }
                    else
                    {
                        if (res > timestamp)
                        {
                            timestamp = res;
                        }
                        // got_no = 1;
                        // printf("Jestem %d 1, dostalem req_answer od %d, ustawiam got_no -> timestamp = %d, ok_got = %d, req_queue = %d %d %d\n", rank, status.MPI_SOURCE, timestamp, ok_got, req_queue[0], req_queue[1], req_queue[2]);
                    }
                }
            }
            else
            {
                cs_size++;
                printf("Jestem %d 1, zwiększam sobie rozmiar ścieżki krytycznej do %d, ok_got = %d -> timestamp = %d, ok_got = %d, req_queue = %d %d %d\n", rank, cs_size, ok_got, timestamp, ok_got, req_queue[0], req_queue[1], req_queue[2]);
            }
        }
    }

    if (rank == ROOT)
    {
        sleep(1);
        int order_id;
        int time_sleep;
        while (1)
        {
            time_sleep = (rand() % (60 - 10 + 1)) + 10;
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
            if (req_queue[rank - 1] == -1)
            {
                ok_got = 0;
                timestamp++;
                req_queue[rank - 1] = timestamp;
                cs_size_req = cs_size;
                printf("Jestem %d 2, wysyłam req do pozostalych procesow -> timestamp = %d, ok_got = %d, req_queue = %d %d %d\n", rank, timestamp, ok_got, req_queue[0], req_queue[1], req_queue[2]);
                for (i = 1; i < size; i++)
                {
                    if (i != rank)
                    {
                        MPI_Send(&req_queue[rank - 1], 1, MPI_INT, i, req, MPI_COMM_WORLD); // pytam o ok
                    }
                }
            }
            while (1)
            {
                // if (got_no == 1)
                // {
                //     printf("Jestem %d 2, otrzymałem NO -> %d\n", rank, timestamp);
                //     ok_got = 0;
                //     got_no = 0;
                //     break;
                // }
                if (ok_got == size - 1 - cs_size)
                {
                    printf("Jestem %d 2, otrzymałem odpowiednia ilosc ok, sprawdzam czy jestem pierwszy -> timestamp = %d, ok_got = %d, req_queue = %d %d %d\n", rank, timestamp, ok_got, req_queue[0], req_queue[1], req_queue[2]);
                    first = -1;
                    for (int i = 0; i < size - 1; i++)
                    {
                        if (first == -1 && req_queue[i] != -1) {
                            first = i;
                        }
                        else if (req_queue[i] != -1 && req_queue[i] < req_queue[first])
                        {
                            first = i;
                        }
                    }
                    // printf("\n%d %d\n\n", rank, first);
                    if (first == rank - 1)
                    {
                        printf("\nJestem %d 2, jestem pierwszy, wchodze do sekcji -> timestamp = %d, ok_got = %d, req_queue = %d %d %d\n\n", rank, timestamp, ok_got, req_queue[0], req_queue[1], req_queue[2]);
                        int tmp = (rand() % (30 - 10 + 1)) + 10;
                        for (i = 25; i > 0; i--)
                        {
                            printf("Jestem %d 2, jestem w sekcji krytycznej przez jeszcze: %d -> timestamp = %d, ok_got = %d, req_queue = %d %d %d\n", rank, i, timestamp, ok_got, req_queue[0], req_queue[1], req_queue[2]);
                            sleep(1);
                        }
                        printf("\nJestem %d 2, wychodzę z sekcji krytycznej -> timestamp = %d, ok_got = %d, req_queue = %d %d %d\n\n", rank, timestamp, ok_got, req_queue[0], req_queue[1], req_queue[2]);
                        
                        for (i = 1; i < size; i++)
                        {
                            if (i != rank)
                            {
                                MPI_Send(&timestamp, 1, MPI_INT, i, rel, MPI_COMM_WORLD);
                            }
                        }
                        cs_size--;
                        ok_got = 0;
                        req_queue[rank - 1] = -1;
                        break;
                    }
                    else
                    {
                        req_queue[first] = -1; // wchodzi first
                        printf("Jestem %d 2, nie jestem pierwszy, abort -> timestamp = %d, ok_got = %d, req_queue = %d %d %d\n", rank, timestamp, ok_got, req_queue[0], req_queue[1], req_queue[2]);
                        // req_queue[rank - 1] = -1;
                        cs_size--;
                        break;
                    }
                }
            }
        }
    }
    MPI_Finalize();
}
