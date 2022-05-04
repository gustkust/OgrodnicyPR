#include <math.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

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
    if (rank == ROOT)
    { // instytut
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
        int timestamp = rank;
        int res = 0;
        int no = -1;
        int ok = 1;
        MPI_Request request;
        MPI_Comm comm;
        while (1)
        {
            printf("Jestem %d, czekam na wiadomosc\n", rank);
            MPI_Recv(&res, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if (res < 5000) // jezeli res < 5000 to znaczy ze wiadomosc jest od innego ziomka i zawiera timestamp
            {
                printf("Jestem %d, otrzymałem wiadomosc z timestampem od %d\n", rank, status.MPI_SOURCE);
                if (timestamp < res) // jezeli moj timestamp jest mniejszy to nie moze wejsc
                {
                    printf("Jestem %d, wysyłam nie do %d\n", rank, status.MPI_SOURCE);
                    MPI_Isend(&no, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &request);
                }
                else // jezeli moj timestamp jest wyzszy to spoko
                {
                    printf("Jestem %d, wysyłam ok do %d\n", rank, status.MPI_SOURCE);
                    MPI_Isend(&ok, 1, MPI_INT, status.MPI_SOURCE, 0, MPI_COMM_WORLD, &request);
                }
            }
            else // jezeli res >= 5000 to znaczy ze to zlecenie od instytutu
            {
                printf("Jestem %d, otrzymałem wiadomosc ze zleceniem\n", rank);
                for (i = 1; i < size; i++) // wysylam prosbe o ok do kazdego ziomka
                {
                    if (i != rank) // poza soba samym
                    {
                        printf("Jestem %d, wysyłam prosbe o OK do %d\n", rank, i);
                        MPI_Send(&timestamp, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
                    }
                }
                int i = 0;
                int got_no = 0;
                while (i != size - 2) // czekam az dostane ok od kazdego ziomka poza soba samym i instytutem
                {
                    res = -2;
                    MPI_Irecv(&res, 1, MPI_INT, i, MPI_ANY_TAG, MPI_COMM_WORLD, &request);
                    if (res == no) // jak dostalem nie to lipa
                    {
                        printf("Jestem %d i dostałem NO\n", rank);
                        got_no = 1;
                    }
                    else if (res == ok) // kal dostalem ok to zwiekszam licznik
                    {
                        i++;
                    }
                }
                if (got_no == 0) // jak dostalem same ok to moge wbijac do sekcji
                {
                    printf("Jestem %d i jestem w sekcji krytycznej\n", rank);
                }
            }

            //     while (1) {
            //         // sleep(1);
            //         MPI_Recv(&res, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            //         for (i = 1; i < size; i++) { // sprawdzam czy ktos juz zdarzyl otrzymac zlecenie i pyta o OK
            //             MPI_Irecv(&res, 1, MPI_INT, i, MPI_ANY_TAG, MPI_COMM_WORLD, &request);
            //             if (res > 5000) break;
            //         }
            //         printf("Jestem %d i moj res to %d\n", rank, res);
            //         if (res <= timestamp) { // jezeli jestem pierwszym co otrzymal zlecenie to res >= 5000 (warunek nie bedzie spelniony) w przeciwnym razie dostaje timestamp
            //             printf("Jestem %d i wysyłam OK procesowi %d!\n", rank, i);
            //             MPI_Send(&timestamp, 1, MPI_INT, i, 0, MPI_COMM_WORLD); // wysylam OK procesowi
            //         } else if (res < 5000) { // jezeli moj timestamp jest mniejszy a res to nie zlecenie
            //             printf("Jestem %d i wysyłam NIE procesowi %d!\n", rank, i);
            //             MPI_Send(&no, 1, MPI_INT, i, 0, MPI_COMM_WORLD); // wysyłam NIE procesowi
            //             break;
            //         }
            //     }

            //     printf("Jestem %d i dostałem info o nowym zleceniu %d\n", rank, res);

            //     for (i = 1; i < size; i++)
            //     {
            //         if (i != rank) {
            //             printf("Jestem %d i wysyłam prośbę o OK do %d\n", rank, i);
            //             MPI_Send(&timestamp, 1, MPI_INT, i, 0, MPI_COMM_WORLD); // wysylam kazdemy swoj timestamp
            //         }
            //     }
            //     i = 1;
            //     while (i != size - 1) { // -1 bo root
            //         if (i != rank) {
            //             printf("Jestem %d i czekam na OK od %d\n", rank, i);
            //             MPI_Recv(&res, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status); // czekam na OK od każdego poza mna i rootem
            //             printf("Jestem %d i dostałem OK od %d\n", rank, i);
            //             if (timestamp < res) timestamp = res + 1; // update timestampu
            //             if (res == -1) {
            //                 printf("Jestem %d i dostałem NIE od %d\n", rank, i);
            //             }
            //         }
            //         i++;
            //     }
            //     printf("Jestem %d i wchodzę do sekcji krytycznej!\n", rank);
        }
    }
    MPI_Finalize();
}
