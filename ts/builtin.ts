import { build } from "./builder";
import { BuilderContext } from "./compiler";
import { CompileError } from "./report";
import { OpCode_alias, OpCode_builtinCall } from "./types";
import utilities from "./utilities";

function makePrimitiveTypeAlias(name: string): OpCode_alias {
	return {
		kind: "alias",
		location: "builtin",
		disableGeneration: true,
		name: name,
		value: {
			kind: "struct",
			location: "builtin",
			id: `builtin:${name}`,
			haveSetId: false,
			caseTag: null,
			doCheck: false,
			fields: [],
			codeBlock: [],
		},
	}
}

export const builtinTypes: OpCode_alias[] = [
	makePrimitiveTypeAlias("Type"),
	makePrimitiveTypeAlias("Bool"),
	makePrimitiveTypeAlias("Number"),
	makePrimitiveTypeAlias("String"),
];

export function builtinCall(context: BuilderContext, callOpCode: OpCode_builtinCall) {
	let index = 0;
	function getBool(): boolean {
		const opCode = build(context, callOpCode.callArguments[index], true);
		index++;
		if (opCode.kind != "bool") {
			throw utilities.TODO();
		}
		return opCode.value;
	}
	
	if (callOpCode.name == "compAssert") {
		const ok = getBool();
		if (!ok) {
			throw new CompileError(`assert failed`)
				.indicator(callOpCode.location, "here");
		}
	} else if (callOpCode.name == "db") {
		debugger;
	} else {
		throw new CompileError(`no builtin named '${callOpCode.name}'`)
			.indicator(callOpCode.location, "here");
	}
}