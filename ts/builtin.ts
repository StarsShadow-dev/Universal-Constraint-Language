import * as utilities from "./utilities.js";
import {
	ASTnode,
	ASTnode_alias,
	ASTnode_argument,
	ASTnode_builtinTask,
	ASTnode_error,
	ASTnode_function,
	ASTnode_identifier,
	ASTnodeType,
	ASTnodeType_selfType,
	ASTnodeType_struct,
	BuilderContext,
	withResolve
} from "./ASTnodes.js";
import { TopLevelDef, unAlias } from "./db.js";
import { CompileError } from "./report.js";

type BuiltinType = ASTnode_alias & { left: ASTnode_identifier, value: ASTnodeType_struct };

function makeBuiltinType(name: string): BuiltinType {
	return new ASTnode_alias(
		"builtin",
		new ASTnode_identifier("builtin", name),
		new ASTnodeType_struct("builtin", "__builtin:" + name, [])
	) as BuiltinType;
}

export function isBuiltinType(type: ASTnodeType, name: string): boolean {
	return isSomeBuiltinType(type) && type.id.split(":")[1] == name;
}

export function isSomeBuiltinType(type: ASTnodeType): boolean {
	return type.id.startsWith("__builtin:");
}

export let builtinTypes: BuiltinType[] = [];
export const builtins = new Map<string, TopLevelDef>();

export function makeListType(T: ASTnodeType): ASTnodeType {
	return new ASTnodeType_struct(
		"builtin",
		`__builtin:List`,
		[
			new ASTnode_argument("builtin", "T", T),
		]
	);
}

export function setUpBuiltins() {
	builtinTypes = [
		makeBuiltinType("Type"),
		makeBuiltinType("Bool"),
		makeBuiltinType("Number"),
		makeBuiltinType("String"),
		makeBuiltinType("Effect"),
		makeBuiltinType("Function"),
		makeBuiltinType("Any"),
		makeBuiltinType("__Unknown__"),
	];
	for (let i = 0; i < builtinTypes.length; i++) {
		const type = builtinTypes[i];
		builtins.set(type.left.name, {
			value: type.value,
		});
	}
	function makeFunction(arg: string, argType: ASTnodeType, body: ASTnode[]): ASTnode_function {
		return new ASTnode_function("builtin",
			new ASTnode_argument("builtin", arg, argType),
			body
		);
	}
	
	function useIdentifier(context: BuilderContext, name: string): ASTnode {
		return new ASTnode_identifier("builtin", name).evaluate(context);
	}
	
	builtins.set("List", {
		value: makeFunction("T", getBuiltinType("Type"),
			[
				new ASTnode_builtinTask((context): ASTnodeType | ASTnode_error => {
					return getBuiltinType("Type");
				}, (context): ASTnode => {
					return withResolve(context, () => {
						// const T = useIdentifier(context, "T");
						const T = unAlias(context, "T");
						debugger;
						if (!(T instanceof ASTnodeType)) {
							return new ASTnode_error("builtin", new CompileError("T is not a type"));
							// return new ASTnodeType_selfType("builtin", "__builtin:List", new )
						}
						return makeListType(T);
					});
				})
			]
		)
	});
}

export function getBuiltinType(name: string): ASTnodeType {
	for (let i = 0; i < builtinTypes.length; i++) {
		const alias = builtinTypes[i];
		if (alias.left.name == name) {
			return alias.value;
		}
	}
	
	utilities.unreachable();
}

// export function evaluateBuiltin(context: BuilderContext, builtinCall: ASTnode_builtinCall): ASTnode {
// 	let index = 0;
// 	function getArg(): ASTnode {
// 		const node = builtinCall.callArguments[index];
// 		if (node == undefined) {
// 			throw new CompileError(`not enough arguments for builtin`)
// 				.indicator(builtinCall.location, "here");
// 		}
// 		index++;
// 		return evaluateList(context, [node])[0];
// 	}
// 	function moreArgs(): boolean {
// 		return index < builtinCall.callArguments.length;
// 	}
// 	function getBool(): boolean {
// 		const opCode = getArg();
// 		if (opCode.kind != "bool") {
// 			utilities.TODO();
// 		}
// 		return opCode.value;
// 	}
// 	function getString(): string {
// 		const opCode = getArg();
// 		if (opCode.kind != "string") {
// 			utilities.TODO();
// 		}
// 		return opCode.value;
// 	}
// 	function getNumber(): number {
// 		const opCode = getArg();
// 		if (opCode.kind != "number") {
// 			utilities.TODO();
// 		}
// 		return opCode.value;
// 	}
	
// 	if (builtinCall.name == "Number") {
// 		const min = getNumber();
// 		const max = getNumber();
// 		return {
// 			kind: "struct",
// 			location: builtinCall.location,
// 			id:  JSON.stringify(builtinCall.location),
// 			fields: [],
// 			data: {
// 				kind: "number",
// 				min: min,
// 				max: max,
// 			},
// 		};
// 	} else if (builtinCall.name == "getError") {
// 		const node = builtinCall.callArguments[index];
// 		if (node == undefined) {
// 			throw new CompileError(`not enough arguments for builtin`)
// 				.indicator(builtinCall.location, "here");
// 		}
// 		index++;
// 		const input = getType(context, node);
		
// 		let output = "";
// 		if (input.kind == "error") {
// 			if (input.compileError) {
// 				output = input.compileError.msg;
// 			} else {
// 				utilities.unreachable();
// 			}
// 		}
		
// 		return {
// 			kind: "string",
// 			location: builtinCall.location,
// 			value: output,
// 		};
// 	} else {
// 		utilities.unreachable();
// 	}
// }