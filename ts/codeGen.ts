// import { OpCode } from "./types";
// import utilities from "./utilities";

// export type CodeGenContext = {
// 	level: number,
// 	modes: string[],
// 	// topLevelText: string,
// };

// // separator
// export const codeGen_sep = codeGenJs_separator;
// export const codeGenList = (context: CodeGenContext, opCodes: OpCode[], newModes?: string[]): string[] => {
// 	let strings = [];
	
// 	context.level++;
// 	if (newModes) {
// 		context.modes.push(...newModes);
// 	}
// 	for (let index = 0; index < opCodes.length; index++) {
// 		const opCode = opCodes[index];
// 		if (opCode.kind == "alias" && opCode.disableGeneration) {
// 			continue;
// 		}
// 		strings.push(codeGen(context, opCode));
// 	}
// 	context.level--;
// 	if (newModes) {
// 		for (let i = 0; i < newModes.length; i++) {
// 			context.modes.pop();
// 		}
// 	}
	
// 	return strings;
// };
// export const codeGen = codeGenJs;

// function codeGenJs_separator(context: CodeGenContext, level: number): string {
// 	let text = "\n";
// 	for (let i = 0; i < level - 1; i++) {
// 		text += "    ";
// 	}
// 	return text;
// }
// function codeGenJs(context: CodeGenContext, opCode: OpCode): string {
// 	const sep = codeGen_sep(context, context.level);
	
// 	switch (opCode.kind) {
// 		case "bool": {
// 			if (opCode.value) {
// 				return "true";
// 			} else {
// 				return "false";
// 			}
// 		}
// 		case "number": {
// 			return `${opCode.value}`;
// 		}
// 		case "string": {
// 			// TODO
// 			return `"${opCode.value}"`;
// 		}
// 		case "identifier": {
// 			return opCode.name;
// 		}
// 		case "call": {
// 			const left = codeGenList(context, [opCode.left]).join(sep);
// 			const callArguments = codeGenList(context, opCode.callArguments).join(", ");
// 			return `${left}(${callArguments})`;
// 		}
// 		case "builtinCall": {
// 			return ``;
// 		}
// 		case "operator": {
// 			let withParentheses = true;
// 			if (opCode.operatorText == ".") {
// 				withParentheses = false;
// 			}
// 			let opText = opCode.operatorText;
			
// 			const left = codeGen(context, opCode.left);
// 			const right = codeGen(context, opCode.right);
			
// 			if (withParentheses) {
// 				return `(${left} ${opText} ${right})`;
// 			} else {
// 				return `${left}${opText}${right}`;
// 			}
// 		}
// 		case "struct": {
// 			let aliasTextList: string[] = [];
// 			for (let i = 0; i < opCode.codeBlock.length; i++) {
// 				const alias = opCode.codeBlock[i];
// 				if (alias.kind == "alias") {
// 					context.level++;
// 					aliasTextList.push(codeGen_sep(context, context.level) + alias.name + ": " + codeGenList(context, [alias.value]) + ",");
// 					context.level--;
// 				}
// 			}
			
// 			return `/*struct*/(() => {${sep}return {${codeGen_sep(context, context.level + 1)}${aliasTextList.join(sep)}${sep}}${codeGen_sep(context, context.level - 1)}})()`;
// 		}
// 		case "enum": {
// 			let aliasTextList: string[] = [];
// 			for (let i = 0; i < opCode.codeBlock.length; i++) {
// 				const alias = opCode.codeBlock[i];
// 				if (alias.kind == "alias") {
// 					context.level++;
// 					aliasTextList.push(codeGen_sep(context, context.level) + alias.name + ": " + codeGenList(context, [alias.value]) + ",");
// 					context.level--;
// 				}
// 			}
// 			return `/*enum*/(() => {${sep}return {${aliasTextList.join(sep)}${sep}}${codeGen_sep(context, context.level - 1)}})()`;
// 		}
// 		case "argument": {
// 			return opCode.name;
// 		}
// 		case "function": {
// 			const functionArguments = codeGenList(context, opCode.functionArguments).join(", ");
// 			const codeBlock = codeGenList(context, opCode.codeBlock).join(sep);
// 			return `(${functionArguments}) => {${sep}${codeBlock}${codeGen_sep(context, context.level - 1)}}`;
// 		}
// 		case "return": {
// 			return "return " + codeGen(context, opCode.expression);
// 		}
// 		case "if": {
// 			const condition = codeGen(context, opCode.condition);
// 			const trueCodeBlock = codeGenList(context, opCode.trueCodeBlock).join(sep);
// 			const falseCodeBlock = codeGenList(context, opCode.falseCodeBlock).join(sep);
// 			return `(${condition} ? ${trueCodeBlock} : ${falseCodeBlock})`;
// 		}
// 		case "instance": {
// 			let textList = [];
// 			for (let i = 0; i < opCode.codeBlock.length; i++) {
// 				const alias = opCode.codeBlock[i];
// 				if (alias.kind != "alias") {
// 					throw utilities.unreachable();
// 				}
				
// 				const value = codeGen(context, alias.value);
				
// 				textList.push(`"${alias.name}": ${value},`);
// 			}
			
// 			let tag = "";
// 			if (opCode.caseTag != null) {
// 				tag = `__tag: ${opCode.caseTag},`;
// 			}
			
// 			if (textList.length == 0) {
// 				return `{${tag}}`;
// 			} else {
// 				return `{${tag}${sep}${textList.join(sep)}${codeGen_sep(context, context.level - 1)}}`;
// 			}
// 		}
// 		case "alias": {
// 			return `let ${opCode.name} = ` + codeGenList(context, [opCode.value]).join(sep);
// 		}
		
// 		default: {
// 			throw utilities.TODO();
// 		}
// 	}
// }