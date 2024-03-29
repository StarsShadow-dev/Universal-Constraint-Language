import {
	TokenType,
	Token,
	
	ASTnode,
} from './types';

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
				throw new Error("error");
			}
		}
	}
	
	return AST;
}