import utilities from "./utilities";
import { Indicator, CompileError } from "./report";
import { BuilderContext, BuilderOptions, CompilerStage } from "./compiler";
import { OpCode, OpCode_alias } from "./types";

export function getAlias(opCodes: OpCode[], name: string): OpCode_alias | null {
	for (let i = 0; i < opCodes.length; i++) {
		const opCode = opCodes[i];
		if (opCode.kind == "alias") {
			if (opCode.name == name) {
				return opCode;
			}
		}
	}
	
	return null;
}

function getTypeOf(context: BuilderContext, opCode: OpCode) {
	
}

export function build(context: BuilderContext, opCodes: OpCode[]) {
	context.file.scope.levels.push(opCodes);
	context.file.scope.currentLevel++;
	
	for (let index = 0; index < opCodes.length; index++) {
		const opCode = opCodes[index];
		
		typeCheck(context, opCode);
	}
	
	context.file.scope.levels.pop();
	context.file.scope.currentLevel--;
}

export function propagateTypes() {}

export function typeCheck(context: BuilderContext, opCode: OpCode) {
	debugger;
	
	switch (opCode.kind) {
		case "alias": {
			build(context, [opCode.value]);
			break;
		}
		case "function": {
			build(context, opCode.codeBlock);
			break;
		}
	}
}