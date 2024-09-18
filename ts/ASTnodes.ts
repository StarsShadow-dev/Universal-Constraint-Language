import * as utilities from "./utilities.js";
import { DB, getAlias, TopLevelDef, unAlias } from "./db.js";
import { SourceLocation } from "./types.js";
import { CompileError } from "./report.js";
import { getBuiltinType, isBuiltinType } from "./builtin.js";
import logger, { LogType } from "./logger.js";

export class BuilderContext {
	aliases: ASTnode_alias[] = [];
	setUnalias: boolean = false;
	
	// Resolve is for a case like this:
	// @x(Number) x + (1 + 1)
	// If the function is a evaluated at top level, I want the AST to stay the same.
	// If resolve always happened the output of that expression would be:
	// @x(Number) x + 2
	// Which is probably fine in this case, but for more complex expressions,
	// having everything be simplified can make things completely unreadable.
	resolve: boolean = true;
	
	callStack: ASTnode_function[] = [];
	
	constructor(
		public db: DB,
	) {}
	
	startCall(fn: ASTnode_function) {
		this.callStack.push(fn);
	}
	
	endCall() {
		this.callStack.pop();
	}
	
	isOnStack(fn: ASTnode_function): boolean {
		return this.callStack.includes(fn);
	}
}

export class CodeGenContext {
	level = 0;
};

export class ASTnode {
	constructor(
		public location: SourceLocation,
	) {}
	
	print(context = new CodeGenContext()): string {
		utilities.TODO();
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		utilities.TODO();
	}
	
	evaluate(context: BuilderContext): ASTnode {
		return this;
	}
}

export class ASTnode_command extends ASTnode {
	constructor(
		location: SourceLocation,
		public text: string,
	) {
		super(location);
	}
	
	print(context = new CodeGenContext()): string {
		return `>${this.text}`;
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		utilities.unreachable();
	}
	
	evaluate(context: BuilderContext): ASTnode {
		utilities.unreachable();
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
	
	print(context = new CodeGenContext()): string {
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
	
	print(context = new CodeGenContext()): string {
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
	
	print(context = new CodeGenContext()): string {
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
	
	print(context = new CodeGenContext()): string {
		if (this.id.startsWith("builtin:")) {
			return this.id.split(":")[1];
		}
		
		const argText = getArgText(context, this.fields);
		return `struct(${argText})`;
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		return getBuiltinType("Type");
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
		public argType: ASTnode,
		public returnType: ASTnode,
	) {
		super(location, id);
	}
	
	print(context = new CodeGenContext()): string {
		const argType = this.argType.print(context);
		const returnType = this.returnType.print(context);
		return `\\(${argType}) -> ${returnType}`;
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		return this;
	}
	
	evaluate(context: BuilderContext): ASTnode {
		let argType = this.argType.evaluate(context);
		if (!(argType instanceof ASTnodeType)) {
			argType = this.argType;
		}
		let returnType = this.returnType.evaluate(context);
		if (!(returnType instanceof ASTnodeType)) {
			returnType = this.returnType;
		}
		return new ASTnodeType_functionType(this.location, this.id, argType, returnType);
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

/**
 * **always replaced by other types**
 * 
 * Occurs if a function is already on the stack during type checking.
 */
// export class ASTnodeType_void extends ASTnodeType {
// 	constructor(
// 		location: SourceLocation,
// 		id: string,
// 	) {
// 		super(location, id);
// 	}
	
// 	print(context = new CodeGenContext()): string {
// 		return `***ASTnodeType_void***`;
// 	}
// }

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
	
	print(context = new CodeGenContext()): string {
		let type = this.arg.type.print(context);
		let body = printAST(context, this.body);
		if (this.body.length == 1 && !body.slice(1).includes("\n")) {
			body = this.body[0].print(context);
		}
		return `@${this.arg.name}(${type}) ${body}`;
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		if (context.isOnStack(this)) {
			return new ASTnode_void(this.location);
		}
		
		const arg = this.arg;
		const argumentTypeType = arg.type.getType(context);
		if (argumentTypeType instanceof ASTnode_error) {
			return argumentTypeType;
		}
		
		let argumentType = arg.type.evaluate(context);
		if (!(argumentType instanceof ASTnodeType)) {
			// TODO: errors?
			argumentType = getBuiltinType("Any");
		}
		context.aliases.push(makeAliasWithType(arg.location, arg.name, argumentType as ASTnodeType));
		context.startCall(this);
		const actualResultType = getTypeFromList(context, this.body);
		context.endCall();
		context.aliases.pop();
		if (actualResultType instanceof ASTnode_error) {
			return actualResultType;
		}
		
		return new ASTnodeType_functionType(this.location, `${JSON.stringify(this.location)}`, argumentType, actualResultType);
	}
	
	evaluate(context: BuilderContext): ASTnode {
		const arg = this.arg;
		
		const oldResolve = context.resolve;
		context.resolve = false;
		const argumentType = arg.type.evaluate(context);
		
		let argValue: ASTnode = new ASTnode_unknowable(arg.location);
		if (argumentType instanceof ASTnodeType) {
			argValue = new ASTnodeType_selfType(arg.location, "TODO?", argumentType);
		}
		
		context.aliases.push(new ASTnode_alias(arg.location, new ASTnode_identifier(arg.location, arg.name), argValue));
		const oldSetUnalias = context.setUnalias;
		context.setUnalias = false;
		const codeBlock = evaluateList(context, this.body);
		context.setUnalias = oldSetUnalias;
		context.aliases.pop();
		
		context.resolve = oldResolve;
		
		const newFunction = new ASTnode_function(this.location, new ASTnode_argument(arg.location, arg.name, argumentType), codeBlock);
		
		logger.log(LogType.evaluate, "newFunction", newFunction.print());
		
		return newFunction;
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
	public def?: TopLevelDef = undefined;
	
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
	
	print(context = new CodeGenContext()): string {
		const left = this.left.print(context);
		const value = this.value.print(context);
		return `${left} = ${value}`;
	}
	
	/**
	 * Currently this gets the type of `value`.
	 */
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		const leftType = this.left.getType(context);
		// If the left side does not give an error, then that means left is already defined.
		if (!(leftType instanceof ASTnode_error)) {
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
	
	print(context = new CodeGenContext()): string {
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
		
		const oldAliases = context.aliases;
		context.aliases = [];
		const value = def.getType(context);
		context.aliases = oldAliases;
		
		return value;
	}
	
	evaluate(context: BuilderContext): ASTnode {
		const value = unAlias(context, this.name);
		if (!value) {
			utilities.unreachable();
			// return new ASTnode_error(this.location,
			// 	new CompileError(`alias '${this.name}' does not exist`)
			// 		.indicator(this.location, `here`)
			// );
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
	
	print(context = new CodeGenContext()): string {
		const left = this.left.print();
		const arg = this.arg.print();
		return `(${left} ${arg})`;
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		// const oldAliases = context.aliases;
		// context.aliases = [];
		const leftType = this.left.getType(context);
		// context.aliases = oldAliases;
		if (leftType instanceof ASTnode_error) {
			return leftType;
		}
		if (!(leftType instanceof ASTnodeType_functionType)) {
			const error = new CompileError(`can not call type ${leftType.print()}`)
				.indicator(this.left.location, `here`);
			return new ASTnode_error(this.location, error);
		}
		
		const functionToCall = this.left.evaluate(context);
		if (!(functionToCall instanceof ASTnode_function)) {
			const returnType = leftType.returnType.getType(context);
			
			return returnType;
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
		
		const newAlias = makeAliasWithType(arg.location, functionToCall.arg.name, functionToCallArgType);
		newAlias.unalias = true;
		context.aliases.push(newAlias);
		const returnType = newNode.getType(context);
		context.aliases.pop();
		
		return returnType;
	}
	
	evaluate(context: BuilderContext): ASTnode {
		const oldResolve = context.resolve;
		context.resolve = true;
		const functionToCall = this.left.evaluate(context);
		context.resolve = oldResolve;
		
		const argValue = this.arg.evaluate(context);
		
		if (functionToCall instanceof ASTnode_function && !context.isOnStack(functionToCall)) {
			const oldSetUnalias = context.setUnalias;
			context.setUnalias = true;
			const arg = functionToCall.arg;
			const newAlias = new ASTnode_alias(arg.location, new ASTnode_identifier(arg.location, arg.name), argValue);
			newAlias.unalias = true;
			context.aliases.push(newAlias);
			if (!context.resolve) context.startCall(functionToCall);
			const resultList = evaluateList(context, functionToCall.body);
			const result = resultList[resultList.length-1];
			if (!context.resolve) context.endCall();
			context.aliases.pop();
			context.setUnalias = oldSetUnalias;
			
			if (context.resolve) {
				return result;
			}
		}
		
		return new ASTnode_call(this.location, this.left.evaluate(context), argValue);
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
	
	needNumber(op: string): boolean {
		return op == "+" ||
		op == "-" ||
		op == "!=" ||
		op == "==" ||
		op == "<=" ||
		op == ">=";
	}
	
	print(context = new CodeGenContext()): string {
		const left = this.left.print(context);
		const right = this.right.print(context);
		if (this.operatorText == ".") {
			return `${left}.${right}`;
		} else {
			return `(${left} ${this.operatorText} ${right})`;
		}
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		const op = this.operatorText;
		
		if (this.needNumber(op)) {
			const left = this.left.getType(context);
			if (left instanceof ASTnode_error) {
				return left;
			}
			const right = this.right.getType(context);
			if (right instanceof ASTnode_error) {
				return right;
			}
			
			{
				let error = expectType(context, getBuiltinType("Number"), left);
				if (error) {
					error.indicator(this.left.location, `on left of '${op}' operator`);
					return new ASTnode_error(this.location, error);
				}
			}
			{
				let error = expectType(context, getBuiltinType("Number"), right);
				if (error) {
					error.indicator(this.right.location, `on right of '${op}' operator`);
					return new ASTnode_error(this.location, error);
				}
			}
			
			return getBuiltinType("Number");
		} else if (op == ".") {
			const leftType = this.left.getType(context);
			if (leftType instanceof ASTnode_error) {
				return leftType;
			}
			
			const left = this.left.evaluate(context);
			if (!(left instanceof ASTnode_instance)) {
				utilities.TODO_addError();
			}
			
			if (!(this.right instanceof ASTnode_identifier)) {
				utilities.TODO_addError();
			}
			const name = this.right.name;
			
			const alias = getAliasFromList(left.codeBlock, name);
			if (!alias) {
				utilities.TODO_addError();
			}
			
			return alias.value.getType(context);
		} else {
			utilities.TODO();
		}
	}
	
	evaluate(context: BuilderContext): ASTnode {
		const op = this.operatorText;
		if (this.needNumber(op)) {
			const left = this.left.evaluate(context);
			const right = this.right.evaluate(context);
			if (!(left instanceof ASTnode_number) || !(right instanceof ASTnode_number) || !context.resolve) {
				// (x + y) knowing x is 1 and not knowing y 
				// -> (1 + y)
				return new ASTnode_operator(this.location, this.operatorText, left, right);
			}
			const left_v = left.value;
			const right_v = right.value;
			
			if (op == "+") {
				return new ASTnode_number(this.location, left_v + right_v);
			} else if (op == "-") {
				return new ASTnode_number(this.location, left_v - right_v);
			} else if (op == "==") {
				return new ASTnode_bool(this.location, left_v == right_v);
			} else if (op == "!=") {
				return new ASTnode_bool(this.location, left_v != right_v);
			} else if (op == "<") {
				return new ASTnode_bool(this.location, left_v < right_v);
			} else if (op == ">") {
				return new ASTnode_bool(this.location, left_v > right_v);
			} else if (op == "<=") {
				return new ASTnode_bool(this.location, left_v <= right_v);
			} else if (op == ">=") {
				return new ASTnode_bool(this.location, left_v >= right_v);
			}
			
			utilities.unreachable();
		} else if (op == ".") {
			const left = this.left.evaluate(context);
			if (!(left instanceof ASTnode_instance)) {
				return this;
			}
			
			if (!(this.right instanceof ASTnode_identifier)) {
				utilities.unreachable();
			}
			const name = this.right.name;
			
			const alias = getAliasFromList(left.codeBlock, name);
			if (!alias) {
				utilities.TODO_addError();
			}
			
			return alias.value;
		} else {
			utilities.TODO();
		}
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
		public trueBody: ASTnode[],
		public falseBody: ASTnode[],
	) {
		super(location);
	}
	
	print(context = new CodeGenContext()): string {
		const condition = this.condition.print();
		const trueBody = printAST(new CodeGenContext(), this.trueBody);
		const falseBody = printAST(new CodeGenContext(), this.falseBody);
		return `if ${condition} then${trueBody}\nelse${falseBody}\n`;
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		const condition = this.condition.getType(context);
		if (condition instanceof ASTnode_error) {
			return condition;
		}
		
		const trueType = getTypeFromList(context, this.trueBody);
		const falseType = getTypeFromList(context, this.falseBody);
		
		if (trueType instanceof ASTnode_void && falseType instanceof ASTnode_void) {
			utilities.TODO_addError();
		}
		if (trueType instanceof ASTnode_void) {
			return falseType;
		}
		if (falseType instanceof ASTnode_void) {
			return trueType;
		}
		
		if (trueType instanceof ASTnode_error) {
			return trueType;
		}
		if (falseType instanceof ASTnode_error) {
			return falseType;
		}
		
		{
			const error = expectType(context, trueType, falseType);
			if (error) {
				const trueLocation = this.trueBody[this.trueBody.length-1].location;
				const falseLocation = this.falseBody[this.falseBody.length-1].location;
				error.indicator(trueLocation, `expected same type as trueBody (${trueType.print()})`);
				error.indicator(falseLocation, `but got type ${falseType.print()}`);
				return new ASTnode_error(this.location, error);
			}
		}
		
		return trueType;
	}
	
	evaluate(context: BuilderContext): ASTnode {
		const condition = this.condition.evaluate(context);
		if (!(condition instanceof ASTnode_bool)) {
			const trueBody = evaluateList(context, this.trueBody);
			const falseBody = evaluateList(context, this.falseBody);
			
			return new ASTnode_if(
				this.location,
				condition,
				trueBody,
				falseBody,
			);
		}
		
		if (condition.value) {
			const resultList = evaluateList(context, this.trueBody);
			return resultList[resultList.length-1];
		} else {
			const resultList = evaluateList(context, this.falseBody);
			return resultList[resultList.length-1];
		}
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

export class ASTnode_void extends ASTnode_error {
	constructor(
		location: SourceLocation,
	) {
		super(location, null);
	}
}

export class ASTnode_unknowable extends ASTnode {
	constructor(
		location: SourceLocation,
	) {
		super(location);
	}
};

//#endregion

//#region utilities

function getArgText(context: CodeGenContext, args: ASTnode[]): string {
	let argText = "";
	for (let i = 0; i < args.length; i++) {
		const argNode = args[i];
		const comma = argText == "";
		argText += argNode.print(context);
		if (comma) {
			argText += ", ";
		}
	}
	return argText;
}

export function makeAliasWithType(location: SourceLocation, name: string, type: ASTnodeType): ASTnode_alias {
	return new ASTnode_alias(
		location,
		new ASTnode_identifier(location, name),
		new ASTnodeType_selfType(location, "TODO?", type)
	);
}

export function getAliasFromList(AST: ASTnode[], name: string): ASTnode_alias | null {
	for (let i = 0; i < AST.length; i++) {
		const alias = AST[i];
		if (!(alias instanceof ASTnode_alias)) {
			utilities.unreachable();
		}
		if (alias.left instanceof ASTnode_identifier && alias.left.name == name) {
			return alias;
		}
	}

	return null;
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
	
	if (expectedType instanceof ASTnodeType_functionType && actualType instanceof ASTnodeType_functionType) {
		if (
			!(expectedType.returnType instanceof ASTnodeType) ||
			!(actualType.returnType instanceof ASTnodeType)
		) {
			utilities.unreachable();
		}
		const error = expectType(context, expectedType.returnType, actualType.returnType);
		if (error != null) {
			error.msg =
			`expected type \"${expectedType.print()}\", but got type \"${actualType.print()}\"\n  return type `
			+ error.msg;
		}
		return error;
	}
	
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
		let nodeText = `\n${AST[i].print(context)}`;
		// text += nodeText.replaceAll("\n", "\n\t");
		for (let c = 0; c < nodeText.length; c++) {
			const char = nodeText[c];
			text += char;
			if (char == "\n" && c != nodeText.length-1) {
				text += "\t";
			}
		}
	}
	context.level--;
	
	return `${text}`;
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