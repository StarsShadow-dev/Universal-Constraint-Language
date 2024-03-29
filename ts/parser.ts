import {
	TokenType,
	Token,
	
	ASTnode,
} from './types';
import { compileError } from "./report";

export function parse(tokens: Token[], i: number): ASTnode[] {
	let AST: ASTnode[] = [];
	
	for (; i < tokens.length; i++) {
		const token = tokens[i];
		
		console.log("token", token)
		
		switch (token.type) {
			case TokenType.number: {
				break;
			}
		
			default: {
				compileError(token.location, `unknown token type ${TokenType[token.type]}`);
				break;
			}
		}
	}
	
	return AST;
}