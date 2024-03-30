import {
	TokenType,
	Token,
	
	ASTnode,
} from "./types";
import { compileError } from "./report";
import utilities from "./utilities";

function needsSemicolon(node: ASTnode): boolean {
	return true;
}

export function parse(tokens: Token[], i: number): ASTnode[] {
	let AST: ASTnode[] = [];
	
	function forward(): Token {
		const token = tokens[i];
		i++
		return token;
	}
	
	while (i < tokens.length) {
		const token = forward();
		
		console.log("token", token)
		
		switch (token.type) {
			case TokenType.number: {
				AST.push({
					type: "number",
					location: token.location,
					value: 0,
				});
				break;
			}
			
			case TokenType.separator: {
				compileError(token.location, "unexpected separator");
				break;
			}
		
			default: {
				utilities.unreachable();
				break;
			}
		}
		
		if (needsSemicolon(AST[AST.length - 1])) {
			const semicolon = forward();
			if (semicolon.type != TokenType.separator || semicolon.text != ";") {
				compileError(semicolon.location, "expected a semicolon");
			}
		}
	}
	
	return AST;
}