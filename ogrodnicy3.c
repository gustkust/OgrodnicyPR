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

    int timestamp = 0;
    int time_in_cs;

    // indekst w kolejce to pid, a wartość to timestamp requestu
    int req_queue[size - 1];
    // jak jest -1 na jakimś indeksie to znaczy, że pid o tym indeksie nie robi requestu
    memset(req_queue, -1, sizeof req_queue);

    // tagi
    int req = 1;
    int rel = 0;
    int req_answer = 2;

    // rozwmiar sekcji krytycznej
    int cs_size = 0;

    int first;

    // wątek do słuchania
    void *listening(void *arg)
    {
        while (1)
        {
            MPI_Recv(&res, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            sleep(1);                   // sleep żeby każdy dostał w tym samym czasie
            if (status.MPI_SOURCE != 0) // jak wiadomość nie jest od instytutu
            {
                if (status.MPI_TAG == req) // jak request
                {
                    if (res > timestamp)
                        timestamp = res; // ewentualny update timestampu

                    timestamp++;
                    MPI_Send(&timestamp, 1, MPI_INT, status.MPI_SOURCE, req_answer, MPI_COMM_WORLD); // wysłanie odpowiedzi na req (timestampu)

                    req_queue[status.MPI_SOURCE - 1] = res; // uaktualnienie kolejki requestów

                    printf("Jestem %d 1, dostalem req od %d, wysylam mu swoj timestamp -> timestamp = %d, req_queue = %d %d %d\n", rank, status.MPI_SOURCE, timestamp, req_queue[0], req_queue[1], req_queue[2]);
                }
                else if (status.MPI_TAG == rel) // jak relese
                {
                    if (res > timestamp)
                        timestamp = res; // ewentualny update timestampu

                    req_queue[status.MPI_SOURCE - 1] = -1; // usunięcie requestu z kolejki

                    printf("Jestem %d 1, dostalem rel od %d -> timestamp = %d, req_queue = %d %d %d\n", rank, status.MPI_SOURCE, timestamp, req_queue[0], req_queue[1], req_queue[2]);
                }
                else if (status.MPI_TAG = req_answer) // jak odpowiedź na request
                {
                    if (res > timestamp)
                        timestamp = res; // ewentualny update timestampu

                    printf("Jestem %d 1, dostalem req_answer od %d -> timestamp = %d, req_queue = %d %d %d\n", rank, status.MPI_SOURCE, timestamp, req_queue[0], req_queue[1], req_queue[2]);
                }
            }
            else // jak wiadomość od instytutu
            {
                cs_size++; // zwiększenie rozmiaru sekcji krytycznej
                printf("Jestem %d 1, zwiększam sobie rozmiar sekcji krytycznej do %d-> timestamp = %d, req_queue = %d %d %d\n", rank, cs_size, timestamp, req_queue[0], req_queue[1], req_queue[2]);
            }
        }
    }

    if (rank == ROOT) // co robi instytut
    {
        sleep(1);
        int order_id = 0;
        int time_sleep;
        while (1)
        {
            time_sleep = (rand() % (30 - 10 + 1)) + 10; // losowanie przerwy między nowymi zleceniami
            printf("Jestem instytut i wysyłam do wszystkich ogrodników info, że jest nowe zlecenie!\n");
            for (i = 1; i < size; i++)
            {
                MPI_Send(&order_id, 1, MPI_INT, i, 0, MPI_COMM_WORLD); // informowanie ogrodników o zleceniach
            }
            for (i = time_sleep; i > 0; i--) // przerwa po nowym sezonie
            {
                printf("%d\n", i);
                sleep(1);
            }
        }
    }
    else // co robi ogrodnik
    {
        // odpalenie wątku ze słuchaniem
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, listening, NULL);

        while (1)
        {
            // jak nie jestem w kolejce
            if (req_queue[rank - 1] == -1)
            {
                // wysyłam do wszystkich request
                timestamp++;
                req_queue[rank - 1] = timestamp;
                printf("Jestem %d 2, wysyłam req do pozostalych procesow -> timestamp = %d, req_queue = %d %d %d\n", rank, timestamp, req_queue[0], req_queue[1], req_queue[2]);
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
                // czekam na miejsce w sekcji krytycznej
                if (cs_size > 0)
                {
                    // sprawdzanie kto jest pierwszy
                    first = -1;
                    for (i = 0; i < size - 1; i++)
                    {
                        if (first == -1 && req_queue[i] != -1)
                        {
                            first = i;
                        }
                        else if (req_queue[i] != -1 && req_queue[i] < req_queue[first])
                        {
                            first = i;
                        }
                    }

                    if (first == rank - 1) // jak ja jestem pierwszy
                    {
                        printf("\nJestem %d 2, jestem pierwszy, wchodze do sekcji -> timestamp = %d, req_queue = %d %d %d\n\n", rank, timestamp, req_queue[0], req_queue[1], req_queue[2]);
                        time_in_cs = (rand() % (30 - 10 + 1)) + 10; // losowanie czasu zlecenia

                        // robienie zlecenia
                        for (i = time_in_cs; i > 0; i--)
                        {
                            printf("Jestem %d 2, jestem w sekcji krytycznej przez jeszcze: %d -> timestamp = %d, req_queue = %d %d %d\n", rank, i, timestamp, req_queue[0], req_queue[1], req_queue[2]);
                            sleep(1);
                        }
                        printf("\nJestem %d 2, wychodzę z sekcji krytycznej -> timestamp = %d, req_queue = %d %d %d\n\n", rank, timestamp, req_queue[0], req_queue[1], req_queue[2]);

                        // wysyłanie relese do wszystkich
                        for (i = 1; i < size; i++)
                        {
                            if (i != rank)
                            {
                                MPI_Send(&timestamp, 1, MPI_INT, i, rel, MPI_COMM_WORLD);
                            }
                        }
                    }
                    else
                    {
                        printf("Jestem %d 2, nie jestem pierwszy, abort -> timestamp = %d, req_queue = %d %d %d\n", rank, timestamp, req_queue[0], req_queue[1], req_queue[2]);
                    }
                    req_queue[first] = -1; // wywalam tego co robi/robił zlecenie z kolejki
                    cs_size--;             // zmniejszam rozmiar sekcji krytycznej (zlecenie zostało zrobione przeze mnie lub ktoś inny je robi)
                    break;
                }
            }
        }
    }
    MPI_Finalize();
}
