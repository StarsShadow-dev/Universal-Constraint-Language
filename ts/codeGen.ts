import { OpCode } from "./types";
import utilities from "./utilities";

export type CodeGenContext = {
	level: number,
	modes: string[],
	// topLevelText: string,
};

// separator
export const codeGen_sep = codeGenJs_separator;
export const codeGenList = (context: CodeGenContext, opCodes: OpCode[], newModes?: string[]): string[] => {
	let strings = [];
	
	context.level++;
	if (newModes) {
		context.modes.push(...newModes);
	}
	for (let index = 0; index < opCodes.length; index++) {
		strings.push(codeGen(context, opCodes[index]));
	}
	context.level--;
	if (newModes) {
		for (let i = 0; i < newModes.length; i++) {
			context.modes.pop();
		}
	}
	
	return strings;
};
export const codeGen = codeGenJs;

function codeGenJs_separator(context: CodeGenContext, level: number): string {
	let text = "\n";
	for (let i = 0; i < level - 1; i++) {
		text += "    ";
	}
	return text;
}
function codeGenJs(context: CodeGenContext, opCode: OpCode): string {
	const sep = codeGen_sep(context, context.level);
	
	switch (opCode.kind) {
		case "bool": {
			if (opCode.value) {
				return "true";
			} else {
				return "false";
			}
		}
		case "number": {
			return `${opCode.value}`;
		}
		case "string": {
			// TODO
			return `"${opCode.value}"`;
		}
		case "identifier": {
			return opCode.name;
		}
		case "call": {
			const left = codeGenList(context, [opCode.left]).join(sep);
			const callArguments = codeGenList(context, opCode.callArguments).join(", ");
			return `${left}(${callArguments})`;
		}
		case "builtinCall": {
			return ``;
		}
		case "argument": {
			return opCode.name;
		}
		case "function": {
			const functionArguments = codeGenList(context, opCode.functionArguments).join(", ");
			const codeBlock = codeGenList(context, opCode.codeBlock).join(sep);
			return `(${functionArguments}) => {${sep}${codeBlock}${codeGen_sep(context, context.level - 1)}}`;
		}
		case "return": {
			return "return " + codeGen(context, opCode.expression);
		}
		case "if": {
			const condition = codeGen(context, opCode.condition);
			const trueCodeBlock = codeGenList(context, opCode.trueCodeBlock).join(sep);
			const falseCodeBlock = codeGenList(context, opCode.falseCodeBlock).join(sep);
			return `(${condition} ? ${trueCodeBlock} : ${falseCodeBlock})`;
		}
		case "newInstance": {
			let textList = [];
			for (let i = 0; i < opCode.codeBlock.length; i++) {
				const alias = opCode.codeBlock[i];
				if (alias.kind != "alias") {
					throw utilities.unreachable();
				}
				
				const value = codeGen(context, alias.value);
				
				textList.push(`"${alias.name}": ${value},`);
			}
			
			return `{${sep}${textList.join(sep)}${codeGen_sep(context, context.level - 1)}}`;
		}
		case "alias": {
			return `let ${opCode.name} = ` + codeGenList(context, [opCode.value]).join(sep);
		}
		
		default: {
			throw utilities.TODO();
		}
	}
}