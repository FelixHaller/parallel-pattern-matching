#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

const int RULE_MEMBERS = 5;
const char BLACK = '#';
const char WHITE = '-';

typedef struct {
	int columns;
	int rows;
	int rulesN;
	int* rules;
} Config;

char* createRectangle(Config* config);
void printConfig(const Config* config);
void printRectangle(const char* rectangle, const int rows, const int columns);
void readConfig(Config* config, const char inputFileName[]);
int* search(const char* rectangle, const int rows, const int columns);

/*
 * main program
 */
int main(int argc, char* argv[]) {
	// MPI variables and initialization
	int size;
	int rank;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	Config* config;
	char* rectangle = NULL;
	int rows;
	int columns;

	if (rank == 0) {
		printf("Starting\n");

		config = &(Config ) { .columns = 0, .rows = 0, .rules = NULL };
		readConfig(config, argv[1]);
		rows = config->rows;
		columns = config->columns;

		printf("Config:\n");
		printConfig(config);

		rectangle = createRectangle(config);

		printf("Rectangle:\n");
		printRectangle(rectangle, rows, columns);
	}

	double start = MPI_Wtime();
	MPI_Bcast(&rows, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&columns, 1, MPI_INT, 0, MPI_COMM_WORLD);

	int rowsPart = (rows + size - 1) / size;

	char* rectanglePart = (char*) malloc(rowsPart * columns * sizeof(char));

	MPI_Scatter(rectangle, rowsPart * columns, MPI_CHAR, rectanglePart,
			rowsPart * columns, MPI_CHAR, 0, MPI_COMM_WORLD);

	if (rank == size - 1 && rows % rowsPart != 0) {
		// number of elements and processes isn't divisible without remainder
		rowsPart = rows % rowsPart;
	}

	int* resultPart = search(rectanglePart, rowsPart, columns);

	int results[size * RULE_MEMBERS];
	MPI_Gather(resultPart, RULE_MEMBERS, MPI_INT, results, RULE_MEMBERS,
	MPI_INT, 0, MPI_COMM_WORLD);

	if (rank == 0) {
		// add offset to the rows
		for (int i = 0; i < size; i++) {
			results[i * RULE_MEMBERS + 1] += i * rowsPart;
			results[i * RULE_MEMBERS + 3] += i * rowsPart;
		}

		int result[RULE_MEMBERS];
		result[0] = 0;
		result[1] = INT_MIN;
		result[2] = INT_MIN;
		result[3] = INT_MAX;
		result[4] = INT_MAX;
		int breaked = 0;
		int nextIsWrong = 0;
		for (int i = 0; i < size; i++) {
			if (results[i * RULE_MEMBERS] == 2) {
				// more then one black rectangle
				breaked = 1;
				break;
			} else if (results[i * RULE_MEMBERS] == 1) {
				if (result[0] == 0) {
					// first black rectangle found
					for (int j = 0; j < RULE_MEMBERS; j++) {
						result[j] = results[i * RULE_MEMBERS + j];
					}
				} else {
					// all other rectangles
					if (nextIsWrong
							|| ((result[0] == 1)
									&& ((result[3] + 1)
											!= (results[i * RULE_MEMBERS + 1])))) {
						// gap between two black rectangles
						breaked = 1;
						break;
					} else if (result[2] != results[i * RULE_MEMBERS + 2]
							|| result[4] != results[i * RULE_MEMBERS + 4]) {
						// vertical shifted rectangles
						breaked = 1;
						break;
					} else {
						result[3] = results[i * RULE_MEMBERS + 3];
						result[4] = results[i * RULE_MEMBERS + 4];
					}
				}
				if ((results[i * RULE_MEMBERS + 3] + 1) % rowsPart != 0) {
					// black rectangle isn't at the end of the part
					nextIsWrong = 1;
				}
			}
		}
		if (breaked) {
			result[0] = 2;
		}

		printf("Time elapsed: %f s\n", MPI_Wtime() - start);

		printf("Final result:\n");
		for (int i = 0; i < RULE_MEMBERS; i++) {
			printf("%d ", result[i]);
		}
		printf("\n");

		if (result[0] == 0) {
			printf("No black rectangle!\n");
		} else if (result[0] == 1) {
			printf("One black rectangle!\n");
			printf("Coordinates:\n");
			for (int i = 1; i < RULE_MEMBERS; i++) {
				printf("%d ", result[i]);
			}
			printf("\n");
		} else if (result[0] == 2) {
			printf("More then one black rectangle!\n");
		}
	}

	free(resultPart);

	if (rank == 0) {
		free(config->rules);
		free(rectangle);

		printf("Finished\n");
	}

	MPI_Finalize();

	return 0;
}

/*
 * create rectangle from config
 */
char* createRectangle(Config* config) {
	char* rectangle = (char*) malloc(
			config->columns * config->rows * sizeof(char));
	for (int n = 0; n < config->rulesN; n++) {
		switch (config->rules[n * RULE_MEMBERS]) {
		// white
		case 0:
			for (int i = config->rules[n * RULE_MEMBERS + 1];
					i <= config->rules[n * RULE_MEMBERS + 3]; i++) {
				for (int j = config->rules[n * RULE_MEMBERS + 2];
						j <= config->rules[n * RULE_MEMBERS + 4]; j++) {
					rectangle[i * config->columns + j] = WHITE;
				}
			}
			break;
			// black
		case 1:
			for (int i = config->rules[n * RULE_MEMBERS + 1];
					i <= config->rules[n * RULE_MEMBERS + 3]; i++) {
				for (int j = config->rules[n * RULE_MEMBERS + 2];
						j <= config->rules[n * RULE_MEMBERS + 4]; j++) {
					rectangle[i * config->columns + j] = BLACK;
				}
			}
			break;
			// toggle
		case 2:
			for (int i = config->rules[n * RULE_MEMBERS + 1];
					i <= config->rules[n * RULE_MEMBERS + 3]; i++) {
				for (int j = config->rules[n * RULE_MEMBERS + 2];
						j <= config->rules[n * RULE_MEMBERS + 4]; j++) {
					if (rectangle[i * config->columns + j] == '-') {
						rectangle[i * config->columns + j] = BLACK;
					} else {
						rectangle[i * config->columns + j] = WHITE;
					}
				}
			}
			break;
		default:
			break;
		}
	}
	return rectangle;
}

/*
 * print the config
 */
void printConfig(const Config* config) {
	printf("%d %d\n", config->rows, config->columns);
	printf("%d\n", config->rulesN);
	for (int i = 0; i < config->rulesN; i++) {
		for (int j = 0; j < RULE_MEMBERS; j++) {
			printf("%d ", config->rules[i * RULE_MEMBERS + j]);
		}
		printf("\n");
	}
}

/*
 * print the rectangle
 */
void printRectangle(const char* rectangle, const int rows, const int columns) {
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++) {
			printf("%c", rectangle[i * columns + j]);
		}
		printf("\n");
	}
}

/*
 * read the specified config file
 */
void readConfig(Config* config, const char inputFileName[]) {
	// open the file
	FILE* inputFile;
	const char* inputMode = "r";
	inputFile = fopen(inputFileName, inputMode);
	if (inputFile == NULL) {
		printf("Could not open input file, exiting!\n");
		exit(1);
	}

	// first line contains number of rows and columns
	int fscanfResult = fscanf(inputFile, "%d %d", &config->rows,
			&config->columns);

	// second line contains number of entries
	fscanfResult = fscanf(inputFile, "%d", &config->rulesN);
	config->rules = (int*) malloc(RULE_MEMBERS * config->rulesN * sizeof(int));

	// all lines after the first contain the entries, values stored as "type r1 c1 r2 c2"
	int type, r1, c1, r2, c2;
	for (int i = 0; i < config->rulesN; i++) {
		fscanfResult = fscanf(inputFile, "%d %d %d %d %d", &type, &r1, &c1, &r2,
				&c2);
		config->rules[i * RULE_MEMBERS] = type;
		config->rules[i * RULE_MEMBERS + 1] = r1;
		config->rules[i * RULE_MEMBERS + 2] = c1;
		config->rules[i * RULE_MEMBERS + 3] = r2;
		config->rules[i * RULE_MEMBERS + 4] = c2;
	}

	fclose(inputFile);

	// EOF result of *scanf indicates an error
	if (fscanfResult == EOF) {
		printf(
				"Something went wrong during reading of config file, exiting!\n");
		exit(1);
	}
}

/*
 * search for black rectangles
 */
int* search(const char* rectangle, const int rows, const int columns) {
	int* result = (int*) malloc(RULE_MEMBERS * sizeof(int));
	result[0] = -1;
	result[1] = INT_MIN;
	result[2] = INT_MIN;
	result[3] = INT_MAX;
	result[4] = INT_MAX;
	int foundStart = 0;
	int foundRowEnd = 0;
	int foundColumnEnd = 0;
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++) {
			if (rectangle[i * columns + j] == WHITE) {
				// white field
				if (foundStart) {
					if (j >= result[2]) {
						if (foundColumnEnd) {
							if (j <= result[4]) {
								if (foundRowEnd) {
									if (i <= result[3]) {
										goto moreThenOne;
									}
								} else {
									if (j == result[2]) {
										// white field under first black field in previos row
										result[3] = i - 1;
										foundRowEnd = 1;
									} else {
										goto moreThenOne;
									}
								}
							}
						} else {
							if (i == result[1]) {
								// same row as first black field, column end found
								result[4] = j - 1;
								foundColumnEnd = 1;
							} else {
								// last element of previous row was a black field
								result[3] = i - 1;
								result[4] = columns - 1;
								foundColumnEnd = 1;
								foundRowEnd = 1;
							}
						}
					}
				}
			} else {
				// black field
				if (foundStart) {
					if (j >= result[2]) {
						if (j == 0 && !foundColumnEnd) {
							// previous row ended with black field
							result[4] = columns - 1;
							foundColumnEnd = 1;
						} else if (foundColumnEnd) {
							if (j <= result[4]) {
								if (foundRowEnd) {
									if (i <= result[3]) {
										// inside of valid region
									} else {
										goto moreThenOne;
									}
								}
							} else {
								goto moreThenOne;
							}
						}
					} else {
						goto moreThenOne;
					}
				} else {
					// first black field, rectangle start found
					result[0] = 1;
					result[1] = i;
					result[2] = j;
					foundStart = 1;
				}
			}
		}
	}
	if (!foundRowEnd) {
		if (rectangle[rows * columns - 1] == BLACK) {
			// valid and last field is black
			result[3] = (rows - 1);
			result[4] = (columns - 1);
			foundColumnEnd = 1;
			foundRowEnd = 1;
		}
	}
	if (!foundRowEnd) {
		if (foundStart && foundColumnEnd) {
			// last row end has valid black fields
			result[3] = (rows - 1);
			foundRowEnd = 1;
		}
	}
	if (!foundStart) {
		// no black field
		result[0] = 0;
	}

	return result;

	moreThenOne: result[0] = 2;
	return result;
}
