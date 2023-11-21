## Errors

An error message should be displayed to the user whenever something goes wrong with the compiling process.

Every error should print text to standard out and then run `exit(1);`.
The functions defined in "error.h" and "error.c" are for automating this when the error is the result of a mistake in the users code.