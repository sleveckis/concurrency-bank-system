/*
 *
 * string_parser.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_parser.h"

#define _GUN_SOURCE

int count_token (char* buf, const char* delim)
{
	//TODO：
	/*
	*	#1.	Check for NULL string
	*	#2.	iterate through string counting tokens
	*		Cases to watchout for
	*			a.	string start with delimeter
	*			b. 	string end with delimeter
	*			c.	account NULL for the last token
	*	#3. return the number of token (note not number of delimeter)
	*/

	/* make a copy so we don't screw with the input string*/
	int count = 0;
	char * bufCopy = (char*)malloc((sizeof(char) * strlen(buf)) + 1);
	strcpy(bufCopy, buf);
	
	/* store the first actual token of "buf" into "token"*/
	char * token = strtok(bufCopy, delim);

	/* iterate through buf to gather how many tokens
	 * there are */
	while (token != NULL){
		count++;
		token = strtok(NULL, delim);
	}
	free(bufCopy);
	return count;

}

command_line str_filler (char* buf, const char* delim)
{
    	command_line line;
	char * save;

	/* strip newlines from input string */
	buf = strtok_r(buf, "\n", &save);

	/* use our copy of buf to count tokens, because token_count
	 * will screw with whatever we pass in*/
	int count = count_token(buf, delim);
	line.num_token = count;


	/* malloc a double pointer, but notice the + 1 is INSIDE the 
	 * parenthesis with count, because we one one entire additional
	 * char * slot, not just an additional byte added on like the +1
	 * additions to malloc above and below*/
	line.command_list = (char**)malloc(sizeof(char*) * (count +1));



	/* store the first actual token of the input line into "token"*/
	char *token = strtok_r(buf, delim, &save);


	/* continue to iterate through the line we are processing,
	 * mallocing a new slot just long enough for the token, plus 1,
	 * into command_list. then copy the token in and move to the next
	 * token in the input line, storing it again in "token"*/
	for(int i = 0; i < count; i++){
		line.command_list[i] = (char*)malloc(sizeof(char) * (strlen(token) )+1);
		strcpy(line.command_list[i], token);
		token = strtok_r(NULL, delim, &save);
	}

	/* make the last slot null, free up our copy buffer*/
	line.command_list[count] = NULL;
	return line;
}


void free_command_line(command_line* command)
{
	//TODO：
	/*
	*	#1.	free the array base num_token
	*/

	for (int i = 0; i < command->num_token; i++){
		free(command->command_list[i]);
	}
	free(command->command_list);
}

