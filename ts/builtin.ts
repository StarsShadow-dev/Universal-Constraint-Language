import path from "path";
import { BuilderContext, FileContext, compileFile } from "./compiler";
import { CompileError } from "./report";
import { OpCode, OpCode_alias, OpCode_builtinCall } from "./types";
import utilities from "./utilities";
import { build } from "./builder";

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

export function builtinCall(context: BuilderContext, callOpCode: OpCode_builtinCall): OpCode {
	let index = 0;
	function getArg(): OpCode {
		const opCode = callOpCode.callArguments[index];
		if (opCode == undefined) {
			throw new CompileError(`not enough arguments for builtin`)
				.indicator(callOpCode.location, "here");
		}
		index++;
		return build(context, opCode, true);
	}
	function getBool(): boolean {
		const opCode = getArg();
		if (opCode.kind != "bool") {
			throw utilities.TODO();
		}
		return opCode.value;
	}
	function getString(): string {
		const opCode = getArg();
		if (opCode.kind != "string") {
			throw utilities.TODO();
		}
		return opCode.value;
	}
	
	if (callOpCode.name == "compAssert") {
		debugger;
		const ok = getBool();
		if (!ok) {
			throw new CompileError(`assert failed`)
				.indicator(callOpCode.location, "here");
		}
	} else if (callOpCode.name == "db") {
		debugger;
	} else if (callOpCode.name == "import") {
		const pathInput = getString();
		const filePath = path.join(path.dirname(context.file.path), pathInput);
		
		let newContext: FileContext;
		try {
			newContext = compileFile(context, filePath, null);
		} catch (error) {
			if (error instanceof CompileError) {
				context.errors.push(error);
				throw "__done__";
			} else {
				throw error;
			}
		}
		
		return {
			kind: "struct",
			location: callOpCode.location,
			id: `${filePath}`,
			haveSetId: false,
			caseTag: null,
			doCheck: false,
			fields: [],
			codeBlock: newContext.opCodes,
		};
	} else {
		throw new CompileError(`no builtin named '${callOpCode.name}'`)
			.indicator(callOpCode.location, "here");
	}
	
	return callOpCode;
}