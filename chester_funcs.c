// chester_funcs.c: Service functions for chester primarily operating
// upon suite_t structs.

#include "chester.h"

////////////////////////////////////////////////////////////////////////////////
// PROBLEM 1 Functions
////////////////////////////////////////////////////////////////////////////////

int suite_create_testdir(suite_t *suite)
{

    struct stat sb; //struct to fill in stat info
    int stats = stat(suite->testdir, &sb);

    //if the directory doesn't exist (checks to make sure it isn't a file)
    if (stats == -1 || (!S_ISDIR(sb.st_mode) && !S_ISREG(sb.st_mode))) 
    {
        mkdir(suite->testdir, S_IRUSR | S_IWUSR | S_IXUSR);
        return 1;
    } //if it is just a regular file with the same name
    else if (S_ISREG(sb.st_mode))
    { 

        printf("ERROR: Could not create test directory '%s'\n", suite->testdir);
        printf("    Non-directory file with that name already exists\n");
        return -1;
    }
    //if it is already a directory it just returns 0
    return 0;
}


int suite_test_set_outfile_name(suite_t *suite, int testnum)
{
    char buffer[100]; //stores name in stack
    sprintf(buffer, "%s/%s-output-%02d.txt", suite->testdir, suite->prefix, testnum);
    suite->tests[testnum].outfile_name = strdup(buffer); //copies to heap 
    return 0; //will always return 0
}

int suite_test_create_infile(suite_t *suite, int testnum)
{
    if (suite->tests[testnum].input == NULL) //no infile to be made
    {
        return 0;
    }
    else
    {
        char buffer[100]; // buffer to hold name of file
        sprintf(buffer, "%s/%s-input-%02d.txt", suite->testdir, suite->prefix, testnum);
        suite->tests[testnum].infile_name = strdup(buffer); // copy over file name
        int fd = open(buffer, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (fd == -1)
        { // if error
            perror("Could not create input file");
            return -1;
        }   //write input to infile
        write(fd, suite->tests[testnum].input, strlen(suite->tests[testnum].input));
        close(fd);
    }
    return 0;
}


int suite_test_read_output_actual(suite_t *suite, int testnum)
{

    struct stat sb; //struct to store stat result
    if (stat(suite->tests[testnum].outfile_name, &sb) == -1)
    {
        perror("Couldn't open file");
        return -1;
    }

    //open the outfile 
    int fd = open(suite->tests[testnum].outfile_name, O_RDONLY);

    if (fd == -1)
    {
        perror("Couldn't open file");
        return -1;
    }

    //malloc extra for null terminator 
    char *contents = malloc(sb.st_size + 1);
    int num = read(fd, contents, sb.st_size);
    if (num == -1) {
        perror("Couldn't read file");
        return -1;
    }
    contents[num] = '\0'; //ends string

    //set output actual to the contents of the file
    suite->tests[testnum].output_actual = contents;

    close(fd);
    return num;
}

////////////////////////////////////////////////////////////////////////////////
// PROBLEM 2 Functions
////////////////////////////////////////////////////////////////////////////////

int suite_test_start(suite_t *suite, int testnum)
{

    suite_test_set_outfile_name(suite, testnum);  //create outfile name

    if (suite_test_create_infile(suite, testnum) != 0) //infile name wasn't created
    {
        exit(TESTFAIL_INPUT);
    }

    pid_t child_pid = fork(); //creates child process 

    if (child_pid == 0)
    { // is child

        int fdo = open(suite->tests[testnum].outfile_name,
         O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR); //create outfile 
        if (fdo == -1) //failure to create outfile
        {
            exit(TESTFAIL_OUTPUT);
        }   //dup fail
        if (dup2(fdo, STDOUT_FILENO) == -1 || dup2(fdo, STDERR_FILENO) == -1)
        {
            exit(TESTFAIL_OUTPUT);
        }

        if (suite->tests[testnum].infile_name != NULL) //tests to ensure that there is an infile
        {
            int fdi = open(suite->tests[testnum].infile_name, O_RDONLY);
            if (dup2(fdi, STDIN_FILENO) == -1)
            {
                exit(TESTFAIL_INPUT);
            }
        }

        char *set_argv[32];
        int set_argc;
        //prepare to exec
        split_into_argv(suite->tests[testnum].program, set_argv, &set_argc);
        execvp(set_argv[0], set_argv); //start running test

        perror("ERROR: test program failed to exec"); //should not get here
        exit(TESTFAIL_EXEC);
    }
    else
    { // parent
        suite->tests[testnum].child_pid = child_pid;
        suite->tests[testnum].state = TEST_RUNNING;
        return 0;
    }

    return 0;
}


int suite_test_finish(suite_t *suite, int testnum, int status)
{

    if (WIFEXITED(status)) //this ensures that the child exited normally
    {
        suite->tests[testnum].exit_code_actual = WEXITSTATUS(status);
    }
    else //fail case
    {   //assigns it to the negative of the signal #
        suite->tests[testnum].exit_code_actual = -1 * WTERMSIG(status);
    }

    suite_test_read_output_actual(suite, testnum); //read output from the test

    if (suite->tests[testnum].output_expect != NULL) //ensures that this isn't a fail test
    {
            //compares outputs and exit codes (expect and actual)
        if ((strcmp(suite->tests[testnum].output_expect, suite->tests[testnum].output_actual) != 0) ||
            (suite->tests[testnum].exit_code_actual != suite->tests[testnum].exit_code_expect))
        {
            suite->tests[testnum].state = TEST_FAILED; 
        }
        else
        {
            suite->tests[testnum].state = TEST_PASSED;
            suite->tests_passed++;
        }
    }
    else //if a test to ensure failure (NULL is expected)
    {
            //only compares exit codes (expect and actual)
        if (suite->tests[testnum].exit_code_actual != suite->tests[testnum].exit_code_expect)
        {
            suite->tests[testnum].state = TEST_FAILED;
        }
        else
        {
            suite->tests[testnum].state = TEST_PASSED;
            suite->tests_passed++;
        }
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
// PROBLEM 3 Functions
////////////////////////////////////////////////////////////////////////////////

void print_window(FILE *out, char *str, int center, int lrwidth)
{

    char buff[100];
    int loc = 0; //ensures that the buff is filled starting at 0

    //traverses through the indexes at center center and width lrwidth
    for (int index = center - lrwidth; index <= center + lrwidth; index++)
    {

        if (index >= 0 && index <= strlen(str)) //ensures real index in str
        {
            buff[loc] = str[index];
            loc++;
        }
    }

    buff[loc] = '\0'; // null terminator required for fprintf

    fprintf(out, "%s", buff);
}


int differing_index(char *strA, char *strB)
{
        //calculates smallest length between input strings
    int minLength = strlen(strA) < strlen(strB) ? strlen(strA) : strlen(strB);

    if (strcmp(strA, strB) == 0) //error case in case they're equal
    {
        return -1;
    }

    for (int index = 0; index < minLength; index++) //finds min index where char's differ
    {
        if (strA[index] != strB[index])
        {
            return index; //immidiately returns index
        }
    }

    return minLength; //only goes here if no difference in characters were found
}


int suite_test_make_resultfile(suite_t *suite, int testnum)
{

    test_t *test = &suite->tests[testnum]; //finds test referred to in param

    char buffer[100];
    sprintf(buffer, "%s/%s-result-%02d.md", suite->testdir, suite->prefix, testnum);
    test->resultfile_name = strdup(buffer); //copys resultfile_name to heap 

    FILE *file = fopen(test->resultfile_name, "w");

    if (file == NULL)
    { // error case
        printf("ERROR: Could not create result file '%s'\n", buffer);
        return -1;
    }

    char *status = (test->state == TEST_PASSED) ? "ok" : "FAIL"; // sets status to "ok" or "FAIL" depending on state

    fprintf(file, "# TEST %d: %s (%s)\n", testnum, test->title, status); // first line
    fprintf(file, "## DESCRIPTION\n");
    fprintf(file, "%s\n\n", test->description);
    fprintf(file, "## PROGRAM: %s\n\n", test->program);

    if (test->input == NULL) //case for no input
    {
        fprintf(file, "## INPUT: None\n\n");
    }
    else //prints input and title
    {
        fprintf(file, "## INPUT:\n");
        fprintf(file, "%s\n\n", test->input);
    }

    // print result for output files
    if (test->output_expect == NULL)
    { // null so no need to check
        fprintf(file, "## OUTPUT: skipped check\n");
    }
    else if ((strcmp(test->output_expect, test->output_actual) == 0))
    {
        fprintf(file, "## OUTPUT: ok\n");
    }
    else
    { // line different
        int diff = differing_index(test->output_actual, test->output_expect);
        fprintf(file, "## OUTPUT: MISMATCH at char position %d\n", diff);
        fprintf(file, "### Expect\n");
        print_window(file, test->output_expect, diff, TEST_DIFFWIDTH);
        fprintf(file, "\n\n### Actual\n");
        print_window(file, test->output_actual, diff, TEST_DIFFWIDTH);
    }

    fprintf(file, "\n\n\n");

    // print result for exit code
    if (test->exit_code_actual == test->exit_code_expect)
    {
        fprintf(file, "## EXIT CODE: ok\n\n");
    }
    else //if there was a mistmatch with exit codes
    {
        fprintf(file, "## EXIT CODE: MISMATCH\n");
        fprintf(file, "- Expect: %d\n", test->exit_code_expect);
        fprintf(file, "- Actual: %d\n\n", test->exit_code_actual);
    }

    if (test->state == TEST_PASSED) //states are same
    {
        fprintf(file, "## RESULT: ok\n");
    }
    else //state mismatch
    {
        fprintf(file, "## RESULT: FAIL\n");
    }

    fclose(file);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// PROBLEM 4 Functions
////////////////////////////////////////////////////////////////////////////////

int suite_run_tests_singleproc(suite_t *suite)
{

    if (suite_create_testdir(suite) == -1) //error case
    {
        printf("ERROR: Failed to create test directory\n");
        return -1;
    }

    printf("Running with single process: ");
    //loops through all tests and runs them 
    for (int testnum = 0; testnum < suite->tests_torun_count; testnum++)
    {

        suite_test_start(suite, suite->tests_torun[testnum]); // starts all tests in the array

        int status;
        int ret = waitpid(suite->tests[suite->tests_torun[testnum]].child_pid, &status, WUNTRACED); // waits for child in above test to finish
        if (ret == -1) {
            perror("Error");
            return -1;
        }
        suite_test_finish(suite, suite->tests_torun[testnum], status); //ends test
        suite_test_make_resultfile(suite, suite->tests_torun[testnum]); //makes file sumarizing test
        printf(".");
    }
    printf(" Done\n");

    return 0;
}


void suite_print_results_table(suite_t *suite)
{

    char state[10];

    //loops through all tests and summarizes results
    for (int index = 0; index < suite->tests_torun_count; index++)
    {

        if (suite->tests[suite->tests_torun[index]].state == TEST_PASSED) //if test passed
        {
            strcpy(state, "ok");
            printf("%2d) %-20s : %s\n", suite->tests_torun[index], suite->tests[suite->tests_torun[index]].title,
                   state);
        }
        else //if test failed
        {
            strcpy(state, "FAIL");
            printf("%2d) %-20s : %s -> see %s\n", suite->tests_torun[index], suite->tests[suite->tests_torun[index]].title,
                   state, suite->tests[suite->tests_torun[index]].resultfile_name);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// PROBLEM 5 Functions
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]);
