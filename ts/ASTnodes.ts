import * as utilities from "./utilities.js";
import { BuilderContext, getAlias, unAlias } from "./db.js";
import { SourceLocation } from "./types.js";
import { CompileError } from "./report.js";
import { getBuiltinType, isBuiltinType } from "./builtin.js";
import logger, { LogType } from "./logger.js";

export class CodeGenContext {
	level = 0;
};

export class ASTnode {
	constructor(
		public location: SourceLocation,
	) {}
	
	print(context: CodeGenContext = new CodeGenContext()): string {
		utilities.TODO();
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		utilities.TODO();
	}
	
	evaluate(context: BuilderContext): ASTnode {
		return this;
	}
}

//#region literals

export class ASTnode_bool extends ASTnode {
	constructor(
		location: SourceLocation,
		public value: boolean,
	) {
		super(location);
	}
	
	print(context: CodeGenContext = new CodeGenContext()): string {
		if (this.value) {
			return "true";
		} else {
			return "false";
		}
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		return getBuiltinType("Bool");
	}
}
export class ASTnode_number extends ASTnode {
	constructor(
		location: SourceLocation,
		public value: number,
	) {
		super(location);
	}
	
	print(context: CodeGenContext = new CodeGenContext()): string {
		return `${this.value}`;
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		return getBuiltinType("Number");
	}
}
export class ASTnode_string extends ASTnode {
	constructor(
		location: SourceLocation,
		public value: string,
	) {
		super(location);
	}
	
	print(context: CodeGenContext = new CodeGenContext()): string {
		return `"${this.value}"`;
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		return getBuiltinType("String");
	}
}
export class ASTnode_list extends ASTnode {
	constructor(
		location: SourceLocation,
		public elements: ASTnode[],
	) {
		super(location);
	}
};

//#endregion

//#region types

export abstract class ASTnodeType extends ASTnode {
	constructor(
		location: SourceLocation,
		public id: string,
	) {
		super(location);
	}
}

export class ASTnodeType_struct extends ASTnodeType {
	constructor(
		location: SourceLocation,
		id: string,
		public fields: ASTnode_argument[],
		public data?: {
			kind: "number",
			min: number,
			max: number,
		},
	) {
		super(location, id);
	}
}

export class ASTnodeType_enum extends ASTnodeType {
	constructor(
		location: SourceLocation,
		id: string,
		public codeBlock: ASTnode[],
	) {
		super(location, id);
	}
}

export class ASTnodeType_functionType extends ASTnodeType {
	constructor(
		location: SourceLocation,
		id: string,
		public arg: ASTnode,
		public returnType: ASTnode,
	) {
		super(location, id);
	}
}

export class ASTnodeType_selfType extends ASTnodeType {
	constructor(
		location: SourceLocation,
		id: string,
		public type: ASTnodeType,
	) {
		super(location, id);
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		return this.type;
	}
}

//#endregion

//#region other

export class ASTnode_function extends ASTnode {
	constructor(
		location: SourceLocation,
		public arg: ASTnode_argument,
		public body: ASTnode[],
	) {
		super(location);
	}
	
	print(context: CodeGenContext = new CodeGenContext()): string {
		let type = this.arg.type.print(context);
		let body = "";
		if (this.body.length == 1) {
			body = this.body[0].print(context);
		} else {
			body = printAST(context, this.body);
		}
		return `@${this.arg.name}(${type}) ${body}`;
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		const arg = this.arg;
		let argumentType = arg.type.evaluate(context);
		if (!(argumentType instanceof ASTnodeType)) {
			// TODO: errors?
			argumentType = getBuiltinType("Any");
		}
		context.aliases.push(makeAliasWithType(arg.location, arg.name, argumentType as ASTnodeType));
		const actualResultType = getTypeFromList(context, this.body);
		context.aliases.pop();
		if (actualResultType instanceof ASTnode_error) {
			return actualResultType;
		}
		
		return new ASTnodeType_functionType(this.location, `${JSON.stringify(this.location)}`, argumentType, actualResultType);
	}
};

export class ASTnode_argument extends ASTnode {
	constructor(
		location: SourceLocation,
		public name: string,
		public type: ASTnode,
	) {
		super(location);
	}
};

export class ASTnode_alias extends ASTnode {
	unalias = false;
	
	constructor(
		location: SourceLocation,
		/**
		 * This is an ASTnode instead of a string
		 * because an ASTnode lets the left side of an alias be something like `a.b.c`.
		 */
		public left: ASTnode,
		public value: ASTnode,
	) {
		super(location);
	}
	
	print(context: CodeGenContext = new CodeGenContext()): string {
		const left = this.left.print(context);
		const value = this.value.print(context);
		return `${left} = ${value}`;
	}
	
	/**
	 * Currently this gets the type of `value`.
	 */
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		const left = this.left.evaluate(context);
		// If the left side does not give us an error, then that means left is already defined.
		if (!(left instanceof ASTnode_error)) {
			return new ASTnode_error(this.location,
				new CompileError(`alias '${this.left.print()}' is already defined`)
					.indicator(this.location, `redefinition here`)
			);
		}
		
		const valueType = this.value.getType(context);
		if (valueType instanceof ASTnode_error) {
			return valueType;
		}
		
		return valueType;
	}
	
	evaluate(context: BuilderContext): ASTnode {
		const value = this.value.evaluate(context);
		
		const out = new ASTnode_alias(this.location, this.left, value);
		out.unalias = this.unalias; // TODO: ?
		return out;
	}
};

export class ASTnode_identifier extends ASTnode {
	constructor(
		location: SourceLocation,
		public name: string,
	) {
		super(location);
	}
	
	print(context: CodeGenContext = new CodeGenContext()): string {
		return `${this.name}`;
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		const def = unAlias(context, this.name);
		if (!def) {
			return new ASTnode_error(this.location,
				new CompileError(`alias '${this.name}' does not exist`)
					.indicator(this.location, `here`)
			);
		}
		
		return def.getType(context);
	}
	
	evaluate(context: BuilderContext): ASTnode {
		const value = unAlias(context, this.name);
		if (!value) {
			return new ASTnode_error(this.location,
				new CompileError(`alias '${this.name}' does not exist`)
					.indicator(this.location, `here`)
			);
		}
		
		if (value instanceof ASTnodeType_selfType) {
			logger.log(LogType.evaluate, `${this.name} = _selfType, can not unAlias`);
			return this;
		}
		
		if (context.resolve) {
			if (value instanceof ASTnode_unknowable) {
				return this;
			} else {
				return value;
			}
		} else {
			const alias = getAlias(context, this.name);
			if (alias && alias.unalias) {
				return value;
			} else {
				return this;
			}
		}
	}
};

export class ASTnode_call extends ASTnode {
	constructor(
		location: SourceLocation,
		public left: ASTnode,
		public arg: ASTnode,
	) {
		super(location);
	}
	
	print(context: CodeGenContext = new CodeGenContext()): string {
		const left = this.left.print();
		const arg = this.arg.print();
		return `(${left} ${arg})`;
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		const leftType = this.left.getType(context);
		if (leftType instanceof ASTnode_error) return leftType;
		if (!(leftType instanceof ASTnodeType_functionType) || !(leftType.returnType instanceof ASTnodeType)) {
			const error = new CompileError(`can not call type ${leftType.print()}`)
				.indicator(this.left.location, `here`);
			return new ASTnode_error(this.location, error);
		}
		
		const functionToCall = this.left.evaluate(context);
		if (!(functionToCall instanceof ASTnode_function)) {
			throw utilities.TODO_addError();
		}
		const functionToCallArgType = functionToCall.arg.type.evaluate(context);
		if (!(functionToCallArgType instanceof ASTnodeType)) {
			utilities.TODO_addError();
		}
		
		const actualArgType = this.arg.getType(context);
		if (actualArgType instanceof ASTnode_error) {
			return actualArgType;
		}
		
		{
			const error = expectType(context, functionToCallArgType, actualArgType);
			if (error) {
				error.indicator(this.location, `for function call here`);
				error.indicator(this.arg.location, `(this argument)`);
				error.indicator(functionToCall.location, `function from here`);
				return new ASTnode_error(this.location, error);
			}
		}
		
		const arg = this.arg;
		
		const newNode = this.evaluate(context);
		context.aliases.push(makeAliasWithType(arg.location, functionToCall.arg.name, functionToCallArgType));
		const returnType = newNode.getType(context);
		context.aliases.pop();
		
		return returnType;
	}
	
	evaluate(context: BuilderContext): ASTnode {
		const oldResolve = context.resolve;
		context.resolve = true;
		const functionToCall = this.left.evaluate(context);
		context.resolve = oldResolve;
		if (!(functionToCall instanceof ASTnode_function)) {
			utilities.TODO();
		}
		
		const oldSetUnalias = context.setUnalias;
		context.setUnalias = true;
		const arg = functionToCall.arg;
		const argValue = this.arg.evaluate(context);
		const newAlias = new ASTnode_alias(arg.location, new ASTnode_identifier(arg.location, arg.name), argValue);
		newAlias.unalias = true;
		context.aliases.push();
		const resultList = evaluateList(context, functionToCall.body);
		const result = resultList[resultList.length-1];
		context.aliases.pop();
		context.setUnalias = oldSetUnalias;
		
		if (context.resolve) {
			return result;
		} else {
			return new ASTnode_call(this.location, this.left.evaluate(context), argValue);
		}
	}
};

export class ASTnode_operator extends ASTnode {
	constructor(
		location: SourceLocation,
		public operatorText: string,
		public left: ASTnode,
		public right: ASTnode,
	) {
		super(location);
	}
};

export class ASTnode_field extends ASTnode {
	constructor(
		location: SourceLocation,
		public name: string,
		public type: ASTnode,
	) {
		super(location);
	}
};

export class ASTnode_match extends ASTnode {
	constructor(
		location: SourceLocation,
		public expression: ASTnode,
		public codeBlock: ASTnode[],
	) {
		super(location);
	}
};

export class ASTnode_matchCase extends ASTnode {
	constructor(
		location: SourceLocation,
		public name: string,
		public types: ASTnode[],
		public codeBlock: ASTnode[],
	) {
		super(location);
	}
};

export class ASTnode_while extends ASTnode {
	constructor(
		location: SourceLocation,
		public condition: ASTnode,
		public codeBlock: ASTnode[],
	) {
		super(location);
	}
};

export class ASTnode_if extends ASTnode {
	constructor(
		location: SourceLocation,
		public condition: ASTnode,
		public codeBlock: ASTnode[],
	) {
		super(location);
	}
};

export class ASTnode_codeBlock extends ASTnode {
	constructor(
		location: SourceLocation,
		public codeBlock: ASTnode[],
	) {
		super(location);
	}
};

export class ASTnode_instance extends ASTnode {
	constructor(
		location: SourceLocation,
		public template: ASTnode,
		public codeBlock: ASTnode[],
	) {
		super(location);
	}
};

//#endregion

//#region internal

export class ASTnode_error extends ASTnode {
	constructor(
		location: SourceLocation,
		/**
		 * `null` If the error has already been handled.
		 */
		public compileError: CompileError | null,
	) {
		super(location);
	}
};

export class ASTnode_unknowable extends ASTnode {
	constructor(
		location: SourceLocation,
	) {
		super(location);
	}
};

//#endregion

//#region utilities

export function makeAliasWithType(location: SourceLocation, name: string, type: ASTnodeType): ASTnode_alias {
	return new ASTnode_alias(
		location,
		new ASTnode_identifier(location, name),
		new ASTnodeType_selfType(location, "TODO?", type)
	);
}

/**
 * Takes an expectedType and an actualType, if the types are different then it throws an error.
 * 
 * @param callBack Used to modify the error message to add more context.
 */
function expectType(
	context: BuilderContext,
	expectedType: ASTnodeType,
	actualType: ASTnodeType,
): CompileError | null {
	if (expectedType instanceof ASTnodeType_selfType) {
		return null;
	}
	
	if (isBuiltinType(expectedType, "Function") && actualType instanceof ASTnodeType_functionType) {
		return null;
	}
	
	// if (expectedType.kind == "functionType" && actualType.kind == "functionType") {
	// 	if (!OpCode_isAtype(expectedType.returnType) || !OpCode_isAtype(actualType.returnType)) {
	// 		utilities.TODO();
	// 	}
	// 	expectType(context, expectedType.returnType, actualType.returnType, (error) => {
	// 		error.msg =
	// 		`expected type \"${OpCodeType_getName(expectedType)}\", but got type \"${OpCodeType_getName(actualType)}\"\n  return type `
	// 		+ error.msg;
			
	// 		callBack(error);
	// 	});
	// 	return;
	// }
	
	// if (expectedType.kind == "enum" && actualType.kind == "struct") {
	// 	for (let i = 0; i < expectedType.codeBlock.length; i++) {
	// 		const alias = expectedType.codeBlock[i];
	// 		if (alias.kind != "alias" || alias.value.kind != "struct") {
	// 			utilities.unreachable();
	// 		}
			
	// 		if (alias.value.id == actualType.id) {
	// 			return;
	// 		}
	// 	}
	// }
	
	if (expectedType.id != actualType.id) {
		const expectedTypeText = expectedType.print();
		const actualTypeText = actualType.print();
		const error = new CompileError(`expected type ${expectedTypeText}, but got type ${actualTypeText}`);
		debugger;
		return error;
	}
	
	return null;
}

export function printAST(context: CodeGenContext, AST: ASTnode[]): string {
	let text = "";
	
	context.level++;
	for (let i = 0; i < AST.length; i++) {
		text += "\n" + "\t".repeat(context.level) + AST[i].print(context);
	}
	context.level--;
	
	return text + "\n" + "\t".repeat(context.level);
}

export function getTypeFromList(context: BuilderContext, AST: ASTnode[]): ASTnodeType | ASTnode_error {
	let outNode = null;
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		if (ASTnode instanceof ASTnode_alias) {
			const valueType = ASTnode.getType(context);
			if (valueType instanceof ASTnode_error) {
				return valueType;
			}
			const name = ASTnode.left.print();
			context.aliases.push(makeAliasWithType(ASTnode.location, name, valueType));
		} else {
			outNode = ASTnode.getType(context);
		}
		// if (ASTnode.kind == "identifier") {
		// 	const alias = getAlias(context, ASTnode.name);
		// 	if (alias) {
		// 		if (alias.unalias) {
		// 			logger.log(LogType.build, `unalias ${ASTnode.name}`);
		// 			AST[i] = alias.value[0];
		// 		} else {
		// 			logger.log(LogType.build, `no unalias ${ASTnode.name}`);
		// 		}
		// 	}
		// }
	}
	
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		if (ASTnode instanceof ASTnode_alias) {
			context.aliases.pop();
		}
	}
	
	if (!outNode) utilities.unreachable();
	return outNode;
}

export function evaluateList(context: BuilderContext, AST: ASTnode[]): ASTnode[] {
	let aliasCount = 0;
	
	let output: ASTnode[] = [];
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		const result: ASTnode = ASTnode.evaluate(context);
		// if (result instanceof ASTnode_alias) {}
		if (result instanceof ASTnode_alias) {
			if (context.setUnalias) {
				result.unalias = true;
			}
			// context.aliases.push(makeAlias(result.location, false, result.name, result));
			context.aliases.push(result);
			aliasCount++;
		}
		output.push(result);
	}
	
	for (let i = 0; i < aliasCount; i++) {
		context.aliases.pop();
	}
	
	return output;
}

//#endregion