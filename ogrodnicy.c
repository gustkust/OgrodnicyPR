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

    int cs_queue[size - 1]; // indeks to rank procesu, wartosc to wartosc timestampu tego procesu,
    // a miejce w kolejce to ilosc procesow z mniejszym timestampem + 1
    memset(cs_queue, 9999, sizeof cs_queue);
    int timestamps[size - 1];
    memset(timestamps, 0, sizeof timestamps);

    int ok = -1;
    int no = -2;
    int cs_size = 0;

    int ok_num = 0; // ilosc otrzymanych ok
    int got_no = 0; // czy dostalem no

    int timestamp = 0;

    void *listening(void *arg)
    {
        while (1)
        {
            printf("Jestem %d 1, czekam na wiadomość\n", rank);
            MPI_Recv(&res, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            printf("Jestem %d 1, dostałem wiadomość %d od %d\n", rank, res, status.MPI_SOURCE);
            sleep(1);
            if (status.MPI_SOURCE != 0)
            { // otrzymano timestamp
                timestamps[status.MPI_SOURCE - 1] = res;
                cs_queue[status.MPI_SOURCE - 1] = res;
                if (res == ok)
                {
                    ok_num++;
                    printf("Jestem %d 1, zwiększam licznik OK z %d do %d\n", rank, ok_num - 1, ok_num);
                }
                else if (res == no)
                {
                    got_no = 1;
                    ok_num = 0;
                    printf("Jestem %d 1, ustawiam, że dostałem NO\n", rank);
                }
                else if (res > timestamp || (res == timestamp && rank > status.MPI_SOURCE))
                {
                    printf("Jestem %d 1, wysyłam OK do %d\n", rank, status.MPI_SOURCE);
                    timestamp = res;
                    MPI_Send(&ok, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD); // jak mam mniejszy to no
                }
                else if (res < timestamp || (res == timestamp && rank < status.MPI_SOURCE))
                {
                    printf("Jestem %d 1, wysyłam NO do %d\n", rank, status.MPI_SOURCE);
                    MPI_Send(&no, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD); // jak mam wiekszy to ok
                }
            }
            else
            {
                printf("Jestem %d 1, zwiększam sobie rozmiar ścieżki krytycznej\n", rank);
                cs_size++;
            }
        }
    }

    if (rank == ROOT)
    { // instytut
        sleep(1);
        int time_sleep = 5;
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
                sleep(20);
            }
        }
    }
    else
    {
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, listening, NULL);
        printf("Jestem %d 2, czekam\n", rank);
        while (1)
        {
            if (cs_size > 0)
            {
                sleep(1);
                for (i = 1; i < size; i++)
                {
                    if (i != rank)
                    {
                        printf("Jestem %d 2, wysyłam prośbę o OK do %d\n", rank, i);
                        MPI_Send(&timestamp, 1, MPI_INT, i, 0, MPI_COMM_WORLD); // pytam o ok
                    }
                }
                while (1)
                {
                    if (got_no == 1)
                    {
                        printf("Jestem %d 2, otrzymałem NO od %d\n", rank, status.MPI_SOURCE);
                        cs_size = 0;
                        break;
                    }
                    if (ok_num == size - 1 - cs_size)
                    {
                        printf("Jestem %d 2, otrzymałem OK od wszsytkich, wchodzę do sekcji krytycznej\n", rank);
                        sleep(2);
                        printf("Jestem %d 2, otrzymałem OK od wszsytkich, wychodzę z sekcji krytycznej\n", rank);
                    }
                }
            }
        }
    }
    MPI_Finalize();
}
