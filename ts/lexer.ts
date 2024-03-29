import { TokenType, Token } from './types';

function wordStart(char: string): boolean {
	return (char >= 'a' && char <= 'z') || (char >= 'A' && char <= 'Z') || char == '_';
}

function wordContinue(char: string): boolean {
	return wordStart(char) || (char >= '0' && char <= '9');
}

export function lex(text: string): Token[] {
	let tokens: Token[] = [];
	
	let line = 1;
	let column = 0;
	let columnStartI = 0
	
	for (let i = 0; i < text.length; i++) {
		const char = text[i];
		
		if (char == "\n") {
			line++;
			column = 0;
			columnStartI = i;
			continue;
		}
		
		column += i - columnStartI;
		
		columnStartI = i;
		const startColumn = column;
		
		// comment
		if (char == '/' && text[i+1] == '/') {
			i += 2;
			for (; i < text.length; i++) {
				console.log("A");
				if (text[i+1] == "\n") {
					break;
				}
			}
		}
		
		// word
		else if (wordStart(char)) {
			let str = "";
			for (; i < text.length; i++) {
				if (wordContinue(text[i])) {
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
		else if (char == "\"") {
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