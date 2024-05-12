import { TokenType, Token } from "./types";

function wordStart(text: string, i: number): boolean {
	return (text[i] >= 'a' && text[i] <= 'z') || (text[i] >= 'A' && text[i] <= 'Z') || text[i] == '_';
}

function wordContinue(text: string, i: number): boolean {
	return wordStart(text, i) || base10Number(text, i);
}

function base10Number(text: string, i: number): boolean {
	return text[i] >= '0' && text[i] <= '9';
}

function oneCharacterOperator(text: string, i: number): boolean {
	return text[i] == '>' ||
	text[i] == '<' ||
	text[i] == '=' ||
	text[i] == '+' ||
	text[i] == '-' ||
	text[i] == '*' ||
	text[i] == '/' ||
	text[i] == '%' ||
	text[i] == '.';
}

function twoCharacterOperator(text: string, i: number): boolean {
	return text[i] == '=' && text[i+1] == '=' ||
	text[i] == '!' && text[i+1] == '=' ||
	text[i] == '&' && text[i+1] == '&' ||
	text[i] == '|' && text[i+1] == '|';
	// text[i] == 'a' && text[i+1] == 's';
}

function separator(text: string, i: number): boolean {
	return text[i] == '(' ||
	text[i] == ')' ||
	text[i] == '{' ||
	text[i] == '}'  ||
	text[i] == '[' ||
	text[i] == ']' ||
	text[i] == ':' ||
	text[i] == ';' ||
	text[i] == ',';
}

export function lex(filePath: string, text: string): Token[] {
	let tokens: Token[] = [];
	
	let line = 1;
	let column = 1;
	let columnStartI = 0
	
	for (let i = 0; i < text.length; i++) {
		if (text[i] == "\n") {
			line++;
			column = 0;
			columnStartI = i;
			continue;
		}
		
		column += i - columnStartI;
		
		columnStartI = i;
		const startColumn = column;
		
		// comment
		if (text[i] == '/' && text[i+1] == '/') {
			let str = "";
			
			i++;
			if (text[i+1] == " ") i++;
			
			for (; i < text.length; i++) {
				if (!text[i+1] || text[i+1] == "\n") {
					break;
				} else {
					str += text[i+1];
				}
			}
			
			if (
				tokens.length > 0 &&
				tokens[tokens.length - 1] &&
				tokens[tokens.length - 1].type == TokenType.comment
			) {
				const lastComment = tokens[tokens.length - 1];
				if (lastComment.location != "builtin" && lastComment.endLine && lastComment.endLine + 1 == line) {
					lastComment.text += "\n" + str;
					lastComment.endLine = line;
					continue;
				}
			}
			
			tokens.push({
				type: TokenType.comment,
				text: str,
				location: {
					path: filePath,
					line: line,
					startColumn: startColumn,
					endColumn: startColumn,
				},
				endLine: line,
			});
		}
		
		else if (text[i] == "'") {
			tokens.push({
				type: TokenType.singleQuote,
				text: text[i],
				location: {
					path: filePath,
					line: line,
					startColumn: startColumn,
					endColumn: startColumn,
				},
			});
		}
		
		else if ((text[i] == "-" && base10Number(text, i + 1)) || base10Number(text, i)) {
			let str = "";
			if (text[i] == "-") {
				str += "-";
				i++;
			}
			for (; i < text.length; i++) {
				if (base10Number(text, i)) {
					str += text[i];	
				} else {
					break;
				}
			}
			
			i--;
			
			tokens.push({
				type: TokenType.number,
				text: str,
				location: {
					path: filePath,
					line: line,
					startColumn: startColumn,
					endColumn: startColumn + str.length - 1,
				},
			});
		}
		
		else if (twoCharacterOperator(text, i)) {
			i++;
			tokens.push({
				type: TokenType.operator,
				text: text[i-1] + text[i],
				location: {
					path: filePath,
					line: line,
					startColumn: startColumn,
					endColumn: startColumn + 1,
				},
			});
		}
		
		else if (oneCharacterOperator(text, i)) {
			tokens.push({
				type: TokenType.operator,
				text: text[i],
				location: {
					path: filePath,
					line: line,
					startColumn: startColumn,
					endColumn: startColumn,
				},
			});
		}
		
		else if (separator(text, i)) {
			tokens.push({
				type: TokenType.separator,
				text: text[i],
				location: {
					path: filePath,
					line: line,
					startColumn: startColumn,
					endColumn: startColumn,
				},
			});
		}
		
		else if (text[i] == "@") {
			tokens.push({
				type: TokenType.builtinIndicator,
				text: text[i],
				location: {
					path: filePath,
					line: line,
					startColumn: startColumn,
					endColumn: startColumn,
				},
			});
		}
		
		// word
		else if (wordStart(text, i)) {
			let str = "";
			for (; i < text.length; i++) {
				if (wordContinue(text, i)) {
					str += text[i];	
				} else {
					break;
				}
			}
			
			i--;
			
			tokens.push({
				type: TokenType.word,
				text: str,
				location: {
					path: filePath,
					line: line,
					startColumn: startColumn,
					endColumn: startColumn + str.length - 1,
				},
			});
		}
		
		// string
		else if (text[i] == "\"") {
			i++;
			let str = "";
			for (; i < text.length; i++) {
				if (text[i] != "\"") { // "
					if (text[i] == "\\") { // \
						if (text[i+1] == "n") {
							str += "\n";
						} else if (text[i+1] == "t") {
							str += "\t";
						} else if (text[i+1] == "\"") { // "
							str += "\"";
						} else if (text[i+1] == "\\") { // \
							str += "\\";
						}
						i++;
						continue;
					}
					str += text[i];	
				} else {
					break;
				}
			}
			
			tokens.push({
				type: TokenType.string,
				text: str,
				location: {
					path: filePath,
					line: line,
					startColumn: startColumn,
					endColumn: startColumn + str.length + 1,
				},
			});
		}
	}
	
	return tokens;
}