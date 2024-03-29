import {
	TokenType,
	Token,
	
	ASTnode,
} from "./types";
import { compileError } from "./report";
import utilities from "./utilities";

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
				utilities.unreachable();
				break;
			}
		}
	}
	
	return AST;
}