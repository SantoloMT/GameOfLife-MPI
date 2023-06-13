#include <stdio.h>
#include <stdlib.h>
#include "mpi.h"
#include <time.h>

//Variabili globali per le funzioni di supporto
int RIGHE;
int COLONNE;

void computeValue( short int *lower, short int *value, short int *upper, short int *temp );
void getRow( int index, short int *grid, short int *row );
void copyRow( short int *sorg, short int *dest );


int main(int argc, char **argv)
{  

    if(argc != 4 )
    {
        fprintf(stderr, "Inserire solo 3 argomenti utilizzando il seguente ordine: 'righe colonne iterazioni'\n");
        exit(1);
    }

    int righe = atoi(argv[1]);
    int colonne = atoi(argv[2]);
    int iterations = atoi(argv[3]);

    RIGHE = righe;
    COLONNE = colonne;
   
    int num_tasks, rank, left, right, rimanenti, righePerTask;
    int *count_send, *disp;
    short int *upperSend, *upperRecv, *lowerSend, *lowerRecv;
    short int *lowerRow, *currentRow, *upperRow, *tmpRow;
    short int *gridSend, *gridRecv, *gridNext, *gridPrint;
    short int random;

    double start;
    double end;
    srand(time(NULL));

    
    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&num_tasks);
    MPI_Status status;

    start = MPI_Wtime(); 
    
    left = (rank - 1 + num_tasks) % num_tasks; 
    right = (rank + 1) % num_tasks; 


    //Calcolo delle righe da dividere per ogni processore
    righePerTask = righe / num_tasks;

    if(righePerTask == 0)
    {
        fprintf(stderr, "Le righe devono essere maggiori o uguali al numero di processori!\n");
        MPI_Finalize();
        exit(1);
    }

    //count_send[i] è un array contenente il numero di celle da inviare ad ogni processore "i". 
    count_send = malloc(sizeof(int) * num_tasks);

    //Rappresenta l'offset del numero di celle da prelevare dalla matrice iniziale "gridSend" e inviare ai processori SLAVES.
    disp = malloc(sizeof(int) * num_tasks);

    //Controllo spazio sufficiente in memoria
    if(count_send == NULL || disp == NULL)
    {
        fprintf(stderr, "Memoria insufficiente per la grandezza della matrice scelta!\n");
        MPI_Finalize();
        exit(1);
    }

    //Calcolo dell'eventuale resto delle righe da dividere.
    rimanenti = righe % num_tasks;
    
    //Calcolo count_send.
    for(int i = 0; i < num_tasks; i++)
    {
        count_send[i] = righePerTask * colonne;
    }

    //Se le righe della matrice non è divisibile per il numero di processori 
    //Le righe in eccesso verrano distribuite equamente a partire dal processo 0 agli altri processori.
    if(rimanenti > 0)
    {
        for( int j = 0; rimanenti > 0 ; j++)
        {
            count_send[j] += colonne;
            rimanenti --;
        }
    }

    //Celle di dati da computare ricevute da ogni singolo processore
    gridRecv = malloc(sizeof(short int) * count_send[rank]);

    //Griglia iniziale generata randomicamente dal master con valori di righe e colonne fornite come argomento dall'utente
    gridSend = malloc(sizeof(short int) * righe * colonne);

    //Griglia che rappresentà la generazione successiva (l'iterazione n+1).
    gridNext = malloc(sizeof(short int) * count_send[rank]);

    //Le righe computate all'iterazione "n" dai singoli processi verranno inviate al master e disposte in ordine nell'array "gridPrint".
    gridPrint = malloc(sizeof(short int) * righe * colonne);

    //Buffer della prima riga del processore da inviare.
    upperSend=malloc(sizeof(short int) * colonne);

    //Buffer della prima riga "fanstasma" del processore ricevuta.
    upperRecv=malloc(sizeof(short int) * colonne);

    //Buffer dell'ultima riga del processore da inviare.
    lowerSend=malloc(sizeof(short int) * colonne);

    //Buffer dell'ultima riga "fantasma" del processore ricevuta.
    lowerRecv=malloc(sizeof(short int) * colonne);

    //Righe da computare per ogni processore
    lowerRow=malloc(sizeof(short int) * colonne);
    currentRow=malloc(sizeof(short int) * colonne);
    upperRow=malloc(sizeof(short int) * colonne);

    //Riga temporanea computata da inserire in "gridNext"
    tmpRow=malloc(sizeof(short int) * colonne);

    //Controllo spazio sufficiente in memoria
    if( gridRecv == NULL || gridSend == NULL || gridNext == NULL || gridPrint == NULL || upperSend == NULL ||
        upperRecv == NULL || lowerSend == NULL || lowerRecv == NULL || lowerRow == NULL || currentRow == NULL || upperRow == NULL || tmpRow == NULL )
    {
        fprintf(stderr, "Memoria insufficiente per la grandezza della matrice scelta!\n");
        exit(1);
    }


    //Calcolo displacements.
    int sum = 0;
    for (int i = 0; i < num_tasks; i++) 
    {
        disp[i] = sum;
        sum += count_send[i];
    }


    if(rank == 0)
    {
        //Inizializza la matrice con valori randomici.
        for(int i = 0; i < righe * colonne ; i++)
        {
            random=rand() % 2;
            gridSend[i] = random;
        }

        printf("GRIGLIA INIZIALE\n");
        for(int i=0;i<righe;i++)
        {
            for(int j=0;j<colonne;j++)
            {
                printf("%d ", gridSend[i * colonne + j ]);
            }
            printf("\n");
        } 
      
    }

    //Divido le righe della matrice iniziale per tutti i processori.
    //Ogni processore ricevera esattamente count_send[rank] dati.
    MPI_Scatterv(gridSend, count_send, disp, MPI_SHORT, gridRecv, count_send[rank] , MPI_SHORT, 0, MPI_COMM_WORLD );

    //Libero la memoria della matrice iniziale che non ci servirà più visto che le righe sono state distribuite ai processori.
    free(gridSend);


    for(int n = 0; n < iterations; n++)
    { 
        //Copio la prima riga della griglia di ogni processore in upperSend
        for(int i=0; i < COLONNE; i++)
        {
            upperSend[i] = gridRecv[i];
        }


        //Copio l'ultima riga della griglia in lowerSend
        int j = 0;
        for(int i = count_send[rank] - COLONNE; j<COLONNE ; i++)
        {
            lowerSend[j] = gridRecv[i];
            j++;
        }

        //Invio la prima riga al processore che mi precede e che la inserirà come ultima riga "fantasma".
        MPI_Send(upperSend, COLONNE, MPI_SHORT, left, 0, MPI_COMM_WORLD); 

        //Invio l'ultima riga al processore che mi segue e che la inserirà come ultima riga "fantasma".
        MPI_Send(lowerSend, COLONNE, MPI_SHORT, right, 0, MPI_COMM_WORLD);

        MPI_Recv(upperRecv, COLONNE, MPI_SHORT, left, 0, MPI_COMM_WORLD, &status);
        MPI_Recv(lowerRecv, COLONNE, MPI_SHORT, right, 0, MPI_COMM_WORLD, &status);



            //Prendo la prima riga e la metto in currentRow
            getRow(0, gridRecv, currentRow);	
            if ( count_send[rank] == COLONNE ) 
            {	//Se ho una riga per processo, la computo e la copio nella gridNext
                computeValue( lowerRecv, currentRow, upperRecv, tmpRow );
                copyRow( tmpRow, gridNext );	

            }
            else {
                //Computo la prima riga
                getRow(1, gridRecv, lowerRow);
                computeValue( lowerRow, currentRow, upperRecv, tmpRow ); 
                copyRow( tmpRow, gridNext );
                
                //Computo l'ultima riga
                //count_send[rank]/COLONNE - 1 == indice dell'ultima riga
                //count_send[rank]/COLONNE - 2 == indice della penultima riga
                getRow(((count_send[rank] / COLONNE) - 2), gridRecv, upperRow);
                getRow(((count_send[rank] / COLONNE) - 1), gridRecv, currentRow);
                computeValue( lowerRecv, currentRow, upperRow, tmpRow ); 

                //Copio l'ultima riga computata nella nuova griglia
                copyRow( tmpRow, &gridNext[count_send[rank] - COLONNE] );
              
                //Se ho più di 2 righe per processo le computo
                if ( (count_send[rank] / COLONNE) > 2 ) 
                {
                    //(count_send[rank] / COLONNE) - 1 <-- perché le righe partono da 0
                    //quindi l'ultima riga corrisponderà al numero delle righe totali - 1.
                    for ( j = 1; j < (count_send[rank] / COLONNE) - 1; j++ )
                    {
                        getRow(j+1, gridRecv, lowerRow);
                        getRow(j, gridRecv, currentRow);
                        getRow(j-1, gridRecv, upperRow);
                        computeValue( lowerRow, currentRow, upperRow, tmpRow );
                        copyRow( tmpRow, &gridNext[ j * COLONNE ] );
                    }
                }
            }

            //Sostituisce i valori della prossima generazione computati nella vecchia griglia
            for (j = 0; j < count_send[rank]; j++ ) 
            {
                gridRecv[j] = gridNext[j];
            }

            //Manda le righe computate da tutti i processi al master
            MPI_Gatherv( gridRecv, count_send[rank], MPI_SHORT, gridPrint, count_send, disp, MPI_SHORT, 0, MPI_COMM_WORLD );

            
            //Il processore master stampa la griglia risultante per ogni iterazione
            if ( rank == 0 ) {
                printf( "\nIteration %d:\n", n+1);
                for ( j = 0; j < RIGHE*COLONNE; j++ ) 
                {
                    printf( "%d ", gridPrint[j] );
                    if ( (j % COLONNE) == (COLONNE - 1) ) 
                    {
                        printf( "\n" );
                    }
                }
            }
            
    }

    if(rank == 0)
    {
        end = MPI_Wtime();
        printf("Il tempo è stato di %2f secondi\n", end - start);
    }

    free(count_send);
    free(disp);
    free(gridRecv);
    free(gridNext);
    free(gridPrint);
    free(upperSend);
    free(upperRecv);
    free(lowerSend);
    free(lowerRecv);
    free(lowerRow);
    free(currentRow);
    free(upperRow);
    free(tmpRow);
   
    MPI_Finalize();

    return EXIT_SUCCESS;

}

//Computazione.
//temp[i] rappresenta il valore di "i" nella prossima generazione.
void computeValue( short int *lower, short int *value, short int *upper, short int *temp ) {
	int i;
	short int vicini[COLONNE];
	
    //Computa quanti vicini ha la cella "i"
	for(i = 0; i < COLONNE; i++) 
    {
		vicini[i] = upper[ (COLONNE + (i) - 1) % COLONNE ] + upper[i] + upper[ (COLONNE + (i) + 1) % COLONNE ]
		                + value[ (COLONNE + (i) - 1) % COLONNE ] + value[ (COLONNE + (i) + 1) % COLONNE ]	
						+ lower[ (COLONNE + (i) - 1) % COLONNE ] + lower[i] + lower[ (COLONNE + (i) + 1) % COLONNE ];
	
		//Se la cella è morta e ha 3 vicini vivi allora diventerà viva nella prossima generazione.
		if ( value[i] == 0 ) 
        {
			if ( vicini[i] == 3 ) 
            {
				temp[i] = 1;
			}
			else 
            {
				temp[i] = 0;
			}
		}
		//Se una cella ha 2 o 3 vicini sopravvive alla prossima generazione, altrimenti morirà.
		else 
        {
			if ( vicini[i] >= 2 && vicini[i] <= 3 ) 
            {
				temp[i] = 1; 
			}
			else 
            {
				temp[i] = 0;
			}
		}
	}	

}

//Restituisce la riga "index" dalla griglia "grid"
void getRow( int index, short int *grid, short int *row ) 
{
	int i;
	for ( i = 0; i < COLONNE; i++ ) 
    {
		row[i] = grid[ (index * COLONNE) + i ];
	}
}


//Copia una riga dalla sorg alla dest
void copyRow( short int *sorg, short int *dest ) 
{
	int i;
	for ( i = 0; i < COLONNE; i++ ) 
    {
		dest[i] = sorg[i];
	}
}