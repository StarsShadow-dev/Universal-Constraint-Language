import * as utilities from "./utilities.js";
import {
	ASTnode,
	ASTnode_alias,
	ASTnode_argument,
	ASTnode_function,
	ASTnode_identifier,
	ASTnode_number,
	ASTnode_operator,
	ASTnodeType,
	ASTnodeType_struct
} from "./ASTnodes.js";
import { TopLevelDef } from "./db.js";

type BuiltinType = ASTnode_alias & { left: ASTnode_identifier, value: ASTnodeType_struct };

function makeBuiltinType(name: string): BuiltinType {
	return new ASTnode_alias(
		"builtin",
		new ASTnode_identifier("builtin", name),
		new ASTnodeType_struct("builtin", "builtin:" + name, [])
	) as BuiltinType;
}

export let builtinTypes: BuiltinType[] = [];
export const builtinFunctions = new Map<string, TopLevelDef>();

export function setUpBuiltins() {
	builtinTypes = [
		makeBuiltinType("Type"),
		makeBuiltinType("Bool"),
		makeBuiltinType("Number"),
		makeBuiltinType("String"),
		makeBuiltinType("Effect"),
		makeBuiltinType("Function"),
		makeBuiltinType("Any"),
	];
	builtinFunctions.set("test", {
		value: new ASTnode_function("builtin",
			new ASTnode_argument("builtin", "n", getBuiltinType("Number")),
			[
				new ASTnode_operator(
					"builtin",
					"+",
					new ASTnode_identifier("builtin", "n"),
					new ASTnode_number("builtin", 1),
				),
			]
		)
	})
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

export function isBuiltinType(type: ASTnodeType, name: string): boolean {
	return type.id.split(":")[1] == name;
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