import { TokenType, Token } from './types';

function wordStart(text: string, i: number): boolean {
	return (text[i] >= 'a' && text[i] <= 'z') || (text[i] >= 'A' && text[i] <= 'Z') || text[i] == '_';
}

function wordContinue(text: string, i: number): boolean {
	return wordStart(text, i) || (text[i] >= '0' && text[i] <= '9');
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
	text[i] == ':' && text[i+1] == ':' ||
	text[i] == '&' && text[i+1] == '&' ||
	text[i] == '|' && text[i+1] == '|' ||
	text[i] == 'a' && text[i+1] == 's';
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

export function lex(text: string): Token[] {
	let tokens: Token[] = [];
	
	let line = 1;
	let column = 0;
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
			i += 2;
			for (; i < text.length; i++) {
				if (text[i+1] == "\n") {
					break;
				}
			}
		}
		
		// else if (twoCharacterOperator(text, i)) {
			
		// }
		
		else if (oneCharacterOperator(text, i)) {
			tokens.push({
				type: TokenType.operator,
				text: text[i],
				location: {
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
				if (text[i] != "\"") {
					str += text[i];	
				} else {
					break;
				}
			}
			
			tokens.push({
				type: TokenType.string,
				text: str,
				location: {
					line: line,
					startColumn: startColumn,
					endColumn: startColumn + str.length + 1,
				},
			});
		}
	}
	
	return tokens;
}