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
    int got_no = 0;

    int im_first;

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

                    printf("Jestem %d 1, dostalem req od %d, wysylam mu swoj timestamp -> %d\n", rank, status.MPI_SOURCE, timestamp);
                }
                else if (status.MPI_TAG == rel)
                {

                    if (res > timestamp)
                    {
                        timestamp = res;
                    }

                    req_queue[status.MPI_SOURCE - 1] = -1;
                    cs_size--;

                    printf("Jestem %d 1, dostalem rel od %d -> %d\n", rank, status.MPI_SOURCE, timestamp);
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
                        printf("Jestem %d 1, dostalem req_answer od %d, zwiekszam ok_got do %d -> %d\n", rank, status.MPI_SOURCE, ok_got, timestamp);
                    }
                    else
                    {
                        if (res > timestamp)
                        {
                            timestamp = res;
                        }
                        got_no = 1;
                        printf("Jestem %d 1, dostalem req_answer od %d, ustawiam got_no -> %d\n", rank, status.MPI_SOURCE, timestamp);
                    }
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
    {
        sleep(1);
        int time_sleep = 120;
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
            ok_got = 0;
            timestamp++;
            req_queue[rank - 1] = timestamp;
            cs_size_req = cs_size;
            printf("Jestem %d 2, wysyłam req do pozostalych procesow -> %d\n", rank, req_queue[rank - 1]);
            for (i = 1; i < size; i++)
            {
                MPI_Send(&req_queue[rank - 1], 1, MPI_INT, i, req, MPI_COMM_WORLD); // pytam o ok
            }
            while (1)
            {
                if (got_no == 1)
                {
                    printf("Jestem %d 2, otrzymałem NO -> %d\n", rank, timestamp);
                    ok_got = 0;
                    got_no = 0;
                    break;
                }
                if (ok_got == size - 1 - cs_size)
                {
                    printf("Jestem %d 2, otrzymałem odpowiednia ilosc ok, sprawdzam czy jestem pierwszy -> %d\n", rank, timestamp);
                    im_first = 1;
                    for (int i = 0; i < size - 1; i++)
                    {
                        if (i != rank && req_queue[i] != -1)
                        {
                            if ((req_queue[i] < req_queue[rank - 1]) || ((req_queue[i] == req_queue[rank - 1] && i < rank - 1)))
                            {
                                im_first = 0;
                            }
                        }
                    }
                    if (im_first)
                    {
                        printf("Jestem %d 2, jestem pierwszy, wchodze do sekcji -> %d\n", rank, timestamp);
                        for (i = 5; i > 0; i--)
                        {
                            printf("Jestem %d 2, jestem w sekcji krytycznej przez jeszcze: %d -> %d\n", rank, i, timestamp);
                            sleep(1);
                        }
                        printf("Jestem %d 2, wychodzę z sekcji krytycznej -> %d\n", rank, timestamp);
                        for (i = 1; i < size; i++)
                        {
                            if (i != rank)
                            {
                                MPI_Send(&timestamp, 1, MPI_INT, i, rel, MPI_COMM_WORLD);
                            }
                        }
                        cs_size--;
                        ok_got = 0;
                        break;
                    }
                    else
                    {
                        printf("Jestem %d 2, nie jestem pierwszy, abort -> %d\n", rank, timestamp);
                        req_queue[rank - 1] = -1;
                        break;
                    }
                }
            }
        }
    }
    MPI_Finalize();
}
