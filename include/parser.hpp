#ifndef PARSER_HPP
#define PARSER_HPP

class Shell;

void parseInput(char *input, Shell &shell);
char *trimWhitespace(char *text);

#endif