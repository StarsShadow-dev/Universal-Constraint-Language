import { OpCode } from "./types";
import utilities from "./utilities";

export type CodeGenContext = {
	level: number,
	modes: string[],
	// topLevelText: string,
};

export const codeGen_separator = codeGenJs_separator;
export const codeGenList = (context: CodeGenContext, opCodes: OpCode[], newModes?: string[]): string => {
	let elements = [];
	
	context.level++;
	if (newModes) {
		context.modes.push(...newModes);
	}
	for (let index = 0; index < opCodes.length; index++) {
		elements.push(codeGen(context, opCodes[index]));
	}
	context.level--;
	if (newModes) {
		for (let i = 0; i < newModes.length; i++) {
			context.modes.pop();
		}
	}
	
	return elements.join(codeGen_separator(context, context.level));
};
export const codeGen = codeGenJs;

function codeGenJs_separator(context: CodeGenContext, level: number): string {
	if (context.modes.includes("arguments")) {
		return ", ";
	} else {
		let text = "\n";
		for (let i = 0; i < level; i++) {
			text += "	";
		}
		return text;
	}
}
function codeGenJs(context: CodeGenContext, opCode: OpCode): string {
	switch (opCode.kind) {
		case "string": {
			// TODO
			return `"${opCode.value}"`;
		}
		case "identifier": {
			return opCode.name;
		}
		case "call": {
			return codeGenList(context, [opCode.left]) +
			"(" + codeGenList(context, opCode.callArguments, ["arguments"]) + ")";
		}
		case "alias": {
			return `let ${opCode.name} = ` + codeGenList(context, [opCode.value]);
		}
		case "argument": {
			return opCode.name;
		}
		case "function": {
			const functionArguments = codeGenList(context, opCode.functionArguments, ["arguments"]);
			const codeBlock = codeGenList(context, opCode.codeBlock);
			return `(${functionArguments}) => ${codeBlock}`;
		}
		
		default: {
			throw utilities.TODO();
		}
	}
}