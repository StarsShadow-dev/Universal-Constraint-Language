import * as utilities from "./utilities";
import { SourceLocation } from "./types";
import { CompileError } from "./report";
import { Token, TokenKind } from "./lexer";

type genericASTnode = {
	location: SourceLocation,
};

// TODO: rm this
export type ASTnode_builtinCall = genericASTnode & {
	kind: "builtinCall",
	name: string,
	callArguments: ASTnode[],
};

export type ASTnode_argument = genericASTnode & {
	kind: "argument",
	name: string,
	type: ASTnode,
};

export type genericASTnodeType = genericASTnode & {
	id: string,
};
export function ASTnode_isAtype(node: ASTnode): node is ASTnodeType {
	if (node.kind == "struct" || node.kind == "enum" || node.kind == "functionType" || node.kind == "_selfType") {
		return true;
	}
	
	return false;
}
export type ASTnodeType =
genericASTnodeType & {
	kind: "struct",
	fields: ASTnode_argument[],
	data?: {
		kind: "number",
		min: number,
		max: number,
	},
} | genericASTnodeType & {
	kind: "enum",
	codeBlock: ASTnode[],
} | genericASTnodeType & {
	kind: "functionType",
	arg: ASTnode,
	returnType: ASTnode,
} | ASTnode_selfType;

export type ASTnode_selfType = genericASTnodeType & {
	kind: "_selfType",
	type: ASTnodeType,
};

export type ASTnode_error = genericASTnode & {
	kind: "error",
	compileError: CompileError | null,
};
export function ASTnode_error_new(location: SourceLocation, error: CompileError | null): ASTnode_error {
	return {
		kind: "error",
		location: location,
		compileError: error,
	}
}

export type ASTnode_function = genericASTnode & {
	kind: "function",
	hasError: boolean,
	arg: ASTnode_argument,
	body: ASTnode[],
};

export type ASTnode_alias = genericASTnode & {
	kind: "alias",
	unalias: boolean,
	name: string,
	value: ASTnode,
};

export type ASTnode =
genericASTnode & {
	kind: "bool",
	value: boolean,
} | genericASTnode & {
	kind: "number",
	value: number,
} | genericASTnode & {
	kind: "string",
	value: string,
} | genericASTnode & {
	kind: "identifier",
	name: string,
} | genericASTnode & {
	kind: "list",
	elements: ASTnode[],
} | genericASTnode & {
	kind: "call",
	left: ASTnode,
	arg: ASTnode,
} |
ASTnode_builtinCall |
genericASTnode & {
	kind: "operator",
	operatorText: string,
	left: ASTnode,
	right: ASTnode,
} | genericASTnode & {
	kind: "field",
	name: string,
	type: ASTnode,
} |
ASTnode_argument |
ASTnodeType |
ASTnode_function |
genericASTnode & {
	kind: "match",
	expression: ASTnode,
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "matchCase",
	name: string,
	types: ASTnode[],
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "while",
	condition: ASTnode,
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "if",
	condition: ASTnode,
	trueBody: ASTnode,
	falseBody: ASTnode,
} | genericASTnode & {
	kind: "codeBlock",
	codeBlock: ASTnode[],
} | genericASTnode & {
	kind: "instance",
	template: ASTnode,
	codeBlock: ASTnode[],
} |
ASTnode_alias
| genericASTnode & {
	kind: "effect",
	type: ASTnode,
}
| genericASTnode & {
	kind: "unknowable",
};

export type ParserContext = {
	tokens: Token[],
	i: number,
}

export enum ParserMode {
	normal,
	single,
	singleNoContinue,
	singleNoOperatorContinue,
	singleNoEqualsOperatorContinue,
	singleNoCall,
	comma,
}

function getOperatorPrecedence(operatorText: string): number {
	if (operatorText == "=") {
		return 1;
	}
	
	else if (operatorText == "||") {
		return 2;
	}
	
	else if (operatorText == "&&") {
		return 3;
	}
	
	else if (
		operatorText == "==" ||
		operatorText == "!=" ||
		operatorText == ">" ||
		operatorText == "<"
	) {
		return 4;
	}
	
	else if (
		operatorText == "+" ||
		operatorText == "-"
	) {
		return 5;
	}
	
	else if (
		operatorText == "*" ||
		operatorText == "/" ||
		operatorText == "%"
	) {
		return 6;
	}
	
	else if (operatorText == "as") {
		return 7;
	}
	
	else if (operatorText == ".") {
		return 8;
	}
	
	else {
		utilities.unreachable();
	}
}

function more(context: ParserContext): boolean {
	return context.tokens[context.i] != undefined;
}

function forward(context: ParserContext): Token {
	const token = context.tokens[context.i];
	
	if (!token) {
		const endToken = context.tokens[context.i-1];
		return {
			kind: TokenKind.endOfFile,
			text: "[end of file]",
			location: endToken.location,
			indentation: 0,
		};
	}
	
	context.i++
	
	return token;
}

function last(context: ParserContext): Token | null {
	const token = context.tokens[context.i-1];
	
	if (token) {
		return token;
	} else {
		return null;
	}
}

function next(context: ParserContext): Token {
	const token = context.tokens[context.i];
	
	if (!token) {
		throw new CompileError("unexpected end of file").indicator(context.tokens[context.i-1].location, "last token here");
	}
	
	return token;
}

function parseOperators(context: ParserContext, left: ASTnode, lastPrecedence: number): ASTnode {
	if (!context.tokens[context.i]) {
		return left;
	}
	
	const nextOperator = context.tokens[context.i];
	
	if (nextOperator.kind == TokenKind.operator) {
		const nextPrecedence = getOperatorPrecedence(nextOperator.text);
		
		if (nextPrecedence > lastPrecedence) {
			let right: ASTnode;
			context.i++;
			let mode;
			if (nextOperator.text == "=") {
				mode = ParserMode.single;
			} else if (nextOperator.text == ".") {
				mode = ParserMode.singleNoContinue;
			} else {
				mode = ParserMode.singleNoOperatorContinue;
			}
			
			const _r = parse(context, mode, nextOperator.indentation + 1, null, getLine(nextOperator))[0];
			right = parseOperators(context, _r, nextPrecedence);
			
			if (nextOperator.text == "=") {
				if (left.kind != "identifier") {
					throw new CompileError("left side of assignment must be an identifier")
						.indicator(left.location, "left side here");
				}
				return {
					kind: "alias",
					location: nextOperator.location,
					unalias: false,
					name: left.name,
					value: right,
				}
			} else {
				return {
					kind: "operator",
					location: nextOperator.location,
					operatorText: nextOperator.text,
					left: left,
					right: right,
				}
			}
		}
	}
	
	return left;
}

function parseType(context: ParserContext, separatingText: string): ASTnode | null {
	let type: ASTnode | null = null;
	
	const separator = next(context);
	if (separator.text == separatingText) {
		forward(context);
		
		type = parse(context, ParserMode.singleNoContinue, separator.indentation + 1, null, getLine(separator))[0];
	}
	
	return type;
}

function parseArgumentList(context: ParserContext): ASTnode_argument[] {
	let AST: ASTnode_argument[] = [];
	
	if (next(context).kind == TokenKind.separator && next(context).text == ")") {
		forward(context);
		return AST;
	}
	
	while (context.i < context.tokens.length) {
		// let comptime = false;
		// if (
		// 	next(context).type == TokenKind.word &&
		// 	next(context).text == "comptime"
		// ) {
		// 	comptime = true;
		// 	forward(context);
		// }
		
		const name = forward(context);
		if (name.kind != TokenKind.word) {
			throw new CompileError("expected name in function arguments").indicator(name.location, "here");
		}
		
		const type = parseType(context, ":");
		
		if (!type) {
			throw new CompileError("function argument without a type").indicator(name.location, "here");
		}
		
		AST.push({
			kind: "argument",
			location: name.location,
			name: name.text,
			type: type,
		});	
		
		const end = forward(context);
		
		if (end.kind == TokenKind.separator && end.text == ")") {
			return AST;
		} else if (end.kind == TokenKind.separator && end.text == ",") {
			continue;
		} else {
			throw new CompileError("expected a comma in function arguments").indicator(end.location, "here");
		}
	}
	
	return AST;
}

function getLine(input: ASTnode | Token): number {
	if (typeof input.location == "string") {
		utilities.unreachable();
	}
	
	return input.location.line;
}

export function parse(context: ParserContext, mode: ParserMode, indentation: number, endAt: ")" | "}" | "]" | null, continueAtLine: number): ASTnode[] {
	let ASTnodes: ASTnode[] = [];
	
	function earlyReturn() {
		if (endAt && more(context) && next(context).kind == TokenKind.separator && next(context).text != ",") {
			if (next(context).text == endAt) {
				forward(context);
			} else {
				utilities.TODO();
			}
		}
	}
	
	function doIndentationCancel(): boolean {
		const lastT = last(context);
		if (lastT) {
			const nextT = next(context);
			if (typeof lastT.location == "string" || typeof nextT.location == "string") {
				utilities.unreachable();
			}
			
			if (lastT.location.line == nextT.location.line) {
				return false;
			}
			
			if (nextT.indentation < indentation) {
				return true;
			}
		}
		
		return false;
	}
	
	// function handleIndentation(nextMode: ParserMode, lastT: Token, nextT: Token | undefined): [ParserMode, number] {
	// 	if (nextT == undefined) {
	// 		return [nextMode, indentation + 1];
	// 	}
		
	// 	if (typeof lastT.location == "string" || typeof nextT.location == "string") {
	// 		utilities.unreachable();
	// 	}
		
	// 	if (lastT.location.line == nextT.location.line) {
	// 		return [ParserMode.single, indentation];
	// 	}
		
	// 	if (nextT.indentation <= lastT.indentation) {
	// 		throw new CompileError("indentation error")
	// 			.indicator(lastT.location, "here")
	// 			.indicator(nextT.location, "next here");
	// 	}
		
	// 	return [nextMode, indentation + 1];
	// }
	
	while (context.i < context.tokens.length) {
		if (endAt != null) {
			const nextToken = next(context);
			
			if (nextToken.kind == TokenKind.separator) {
				if (nextToken.text == ")" || nextToken.text == "}" || nextToken.text == "]") {
					if (endAt == nextToken.text) {
						context.i++;
						return ASTnodes;
					} else {
						throw new CompileError("unexpected separator")
							.indicator(nextToken.location, `expected '${endAt} but got '${nextToken.text}'`);
					}
				}
			}
		}
		
		// indentation canceling
		if (doIndentationCancel()) {
			earlyReturn();
			return ASTnodes;
		}
		
		if (
			mode != ParserMode.singleNoCall &&
			ASTnodes.length > 0 &&
			more(context) &&
			next(context).kind != TokenKind.operator &&
			ASTnodes[ASTnodes.length - 1].kind != "alias" &&
			!(
				next(context).kind == TokenKind.separator &&
				next(context).text == ")"
			)
		) {
			const left = ASTnodes.pop();
			if (!left) {
				utilities.unreachable();
			}
			
			debugger;
			const arg = parse(context, ParserMode.singleNoCall, next(context).indentation + 1, null, getLine(left))[0];
			if (arg == undefined) {
				ASTnodes.push(left); // undo pop
				// if (doIndentationCancel()) {
				// 	earlyReturn();
				// 	return ASTnodes;
				// }
			} else {
				ASTnodes.push({
					kind: "call",
					location: left.location,
					left: left,
					arg: arg,
				});
				continue;
			}
		}
		
		if (
			mode == ParserMode.single &&
			ASTnodes.length > 0 &&
			next(context).kind != TokenKind.operator
		) {
			earlyReturn();
			return ASTnodes;
		}
		
		const token = forward(context);
		const nextIndentation = token.indentation + 1;
		
		switch (token.kind) {
			case TokenKind.endOfFile: {
				return ASTnodes;
			}
			
			case TokenKind.comment: {
				continue;
			}
			
			case TokenKind.number: {
				ASTnodes.push({
					kind: "number",
					location: token.location,
					value: Number(token.text),
				});
				break;
			}
			
			case TokenKind.string: {
				ASTnodes.push({
					kind: "string",
					location: token.location,
					value: token.text,
				});
				break;
			}
			
			case TokenKind.word: {
				if (token.text == "true" || token.text == "false") {
					ASTnodes.push({
						kind: "bool",
						location: token.location,
						value: token.text == "true",
					});
				}
				
				else if (token.text == "field") {
					const name = forward(context);
					if (name.kind != TokenKind.word) {
						throw new CompileError("expected name").indicator(name.location, "here");
					}
					
					const type = parseType(context, ":");
					if (!type) {
						throw new CompileError("a field must have a type").indicator(name.location, "here");
					}
					
					ASTnodes.push({
						kind: "field",
						location: token.location,
						name: name.text,
						type: type,
					});
				}
				
				else if (token.text == "if") {
					utilities.TODO();
					// const openingParentheses = forward(context);
					// if (openingParentheses.type != TokenKind.separator || openingParentheses.text != "(") {
					// 	throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
					// }
					
					// const condition = parse(context, ParserMode.single, ")")[0];
					// if (!condition) {
					// 	throw new CompileError("if expression is missing a condition").indicator(token.location, "here");
					// }
					
					// ASTnodes.push({
					// 	kind: "if",
					// 	location: token.location,
					// 	condition: condition,
					// 	trueBody: codeBlock,
					// 	falseBody: falseCodeBlock,
					// });	
				}
				
				else if (token.text == "struct") {
					const openingParentheses = forward(context);
					if (openingParentheses.kind != TokenKind.separator || openingParentheses.text != "(") {
						throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
					}
					const fields = parseArgumentList(context);
					
					// const openingBracket = forward(context);
					// if (openingBracket.type != TokenKind.separator || openingBracket.text != "{") {
					// 	throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					// }
					// const codeBlock = parse(context, ParserMode.normal, "}");
					
					ASTnodes.push({
						kind: "struct",
						location: token.location,
						id: JSON.stringify(token.location),
						fields: fields,
					});
				}
				
				else if (token.text == "enum") {
					const openingBracket = forward(context);
					if (openingBracket.kind != TokenKind.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					const codeBlock = parse(context, ParserMode.normal, indentation, "}", getLine(token));
					for (let i = 0; i < codeBlock.length; i++) {
						const ASTnode = codeBlock[i];
						if (ASTnode.kind == "identifier") {
							codeBlock[i] = {
								kind: "alias",
								location: ASTnode.location,
								unalias: false,
								name: ASTnode.name,
								value: {
									kind: "struct",
									location: ASTnode.location,
									id: JSON.stringify(token.location),
									fields: [],
								},
							};
						}
					}
					
					ASTnodes.push({
						kind: "enum",
						location: token.location,
						id: JSON.stringify(token.location),
						codeBlock: codeBlock,
					});
				}
				
				// else if (token.text == "match") {
				// 	const expression = parse(context, ParserMode.single, indentation, null, getLine(token))[0];
					
				// 	const openingBracket = forward(context);
				// 	if (openingBracket.kind != TokenKind.separator || openingBracket.text != "{") {
				// 		throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
				// 	}
				// 	const codeBlock = parse(context, ParserMode.normal, indentation, "}", getLine(token));
					
				// 	ASTnodes.push({
				// 		kind: "match",
				// 		location: token.location,
				// 		expression: expression,
				// 		codeBlock: codeBlock,
				// 	});
				// }
				
				else {
					ASTnodes.push({
						kind: "identifier",
						location: token.location,
						name: token.text,
					});
					debugger;
				}
				break;
			}
			
			case TokenKind.separator: {
				if (token.text == endAt) {
					return ASTnodes;
				}
				if (token.text == "(") {
					const elements = parse(context, ParserMode.comma, nextIndentation, ")", getLine(token));
					if (elements.length == 0) {
						utilities.unreachable();
					} else if (elements.length > 1) {
						utilities.TODO();
					} else {
						ASTnodes.push(elements[0]);
					}
				} else if (token.text == "[") {
					const elements = parse(context, ParserMode.comma, nextIndentation, "]", getLine(token));
					
					ASTnodes.push({
						kind: "list",
						location: token.location,
						elements: elements,
					});
				} else if (token.text == "@") {
					const name = forward(context);
					if (name.kind != TokenKind.word) {
						throw new CompileError("expected name for function argument").indicator(token.location, "here");
					}
					
					const openingParentheses = forward(context);
					if (openingParentheses.kind != TokenKind.separator || openingParentheses.text != "(") {
						throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
					}
					
					const type = parse(context, ParserMode.normal, nextIndentation, ")", getLine(token))[0];
					if (!type) {
						throw new CompileError("function argument without a type").indicator(name.location, "here");
					}
					
					debugger;
					const body = parse(context, ParserMode.normal, nextIndentation, null, getLine(token));
					ASTnodes.push({
						kind: "function",
						location: token.location,
						hasError: false,
						arg: {
							kind: "argument",
							location: token.location,
							name: name.text,
							type: type,
						},
						body: body,
					});
				} else if (token.text == "&") {
					const template = parse(context, ParserMode.single, nextIndentation, null, getLine(token))[0];
					if (!template) {
						utilities.TODO_addError();
					}
					
					const openingBracket = forward(context);
					if (openingBracket.kind != TokenKind.separator || openingBracket.text != "{") {
						throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
					}
					const codeBlock = parse(context, ParserMode.normal, nextIndentation, "}", getLine(token));
					
					ASTnodes.push({
						kind: "instance",
						location: token.location,
						template: template,
						codeBlock: codeBlock,
					});
				} else if (token.text == "\\") {
					utilities.TODO();
					// const openingParentheses = forward(context);
					// if (openingParentheses.type != TokenKind.separator || openingParentheses.text != "(") {
					// 	throw new CompileError("expected openingParentheses").indicator(openingParentheses.location, "here");
					// }
					
					// const functionArguments = parse(context, ParserMode.comma, ")");
					// const returnType = parseType(context, "->");
					// if (!returnType) {
					// 	throw new CompileError("function type must have a return type").indicator(token.location, "here");
					// }
					
					// ASTnodes.push({
					// 	kind: "functionType",
					// 	location: token.location,
					// 	id: JSON.stringify(token.location),
					// 	functionArguments: functionArguments,
					// 	returnType: returnType,
					// });
				} else {
					if (endAt == null) {
						context.i--;
						return ASTnodes;
					} else {
						throw new CompileError("unexpected separator").indicator(token.location, "here");
					}
				}
				break;
			}
			case TokenKind.operator: {
				const left = ASTnodes[ASTnodes.length-1];
				if (left == undefined) {
					utilities.TODO();
				} else {
					if (token.text == "->") {
						utilities.TODO();
						// const identifier = ASTnodes.pop();
						// if (!identifier || (identifier.kind != "identifier" && identifier.kind != "call")) {
						// 	utilities.TODO();
						// }
						
						// const openingBracket = forward(context);
						// if (openingBracket.type != TokenKind.separator || openingBracket.text != "{") {
						// 	throw new CompileError("expected openingBracket").indicator(openingBracket.location, "here");
						// }
						// const codeBlock = parse(context, ParserMode.normal, "}");
						
						// let name = "";
						// let types: ASTnode[] = [];
						
						// if (identifier.kind == "identifier") {
						// 	name = identifier.name;
						// } else {
						// 	if (identifier.left.kind != "identifier") {
						// 		utilities.TODO();
						// 	}
						// 	name = identifier.left.name;
						// 	types = identifier.arg;
						// }
						
						// ASTnodes.push({
						// 	kind: "matchCase",
						// 	location: identifier.location,
						// 	name: name,
						// 	types: types,
						// 	codeBlock: codeBlock,
						// });
					} else {
						context.i--;
						ASTnodes.pop();
						ASTnodes.push(parseOperators(context, left, 0));
					}
				}
				break;
			}
		
			default: {
				utilities.unreachable();
			}
		}
		
		if (mode == ParserMode.singleNoContinue) {
			earlyReturn();
			return ASTnodes;
		}
		
		if (mode == ParserMode.singleNoOperatorContinue) {
			earlyReturn();
			return ASTnodes;
		}
		
		if (context.tokens[context.i] && next(context).kind == TokenKind.operator) {
			if (mode == ParserMode.singleNoEqualsOperatorContinue && next(context).text == "=") {
				earlyReturn();
				return ASTnodes;
			}
			continue;
		}
		
		if (mode == ParserMode.singleNoEqualsOperatorContinue) {
			earlyReturn();
			return ASTnodes;
		}
		
		if (mode == ParserMode.singleNoCall) {
			earlyReturn();
			return ASTnodes;
		}
		
		if (
			ASTnodes.length > 0 &&
			more(context) &&
			next(context).kind != TokenKind.operator &&
			!(next(context).kind == TokenKind.separator && next(context).text == ",")
		) {
			continue; // continue for fn call at top of loop
		}
		
		if (mode == ParserMode.comma) {
			const comma = forward(context);
			if (comma.kind != TokenKind.separator || comma.text != ",") {
				throw new CompileError("expected a comma").indicator(comma.location, "here");
			}
		}
	}
	
	return ASTnodes;
}