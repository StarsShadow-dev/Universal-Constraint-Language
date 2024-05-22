import utilities from "./utilities";
import { callFunction } from "./builder";
import { BuilderContext } from "./compiler";
import { ScopeObject } from "./types";
import {
	getBool,
	getNumber,
	getString,
	onCodeGen
} from "./builtin";
import {
	CodeGenText,
	getCGText,
	unwrapScopeObject,
} from "./types";

export default {
	startExpression(dest: CodeGenText, context: BuilderContext) {
		if (context.inIndentation && onCodeGen["startExpression"] && context.file.scope.generatingFunction) {
			callFunction(context, onCodeGen["startExpression"], [
				getNumber(context.file.scope.generatingFunction.indentation)
			], "builtin", true, null, dest, null);
		}
	},
	
	endExpression(dest: CodeGenText, context: BuilderContext) {
		if (context.inIndentation && onCodeGen["endExpression"] && context.file.scope.generatingFunction) {
			callFunction(context, onCodeGen["endExpression"], [], "builtin", true, null, dest, null);
		}
	},
	
	start(context: BuilderContext) {
		if (onCodeGen["start"]) {
			callFunction(context, onCodeGen["start"], [], "builtin", true, null, context.topCodeGenText, null);
		}
	},
	
	bool(dest: CodeGenText, context: BuilderContext, value: boolean) {
		if (onCodeGen["bool"]) {
			callFunction(context, onCodeGen["bool"], [
				getBool(value)
			], "builtin", true, null, dest, null);
		}
	},
	
	number(dest: CodeGenText, context: BuilderContext, value: number) {
		if (onCodeGen["number"]) {
			callFunction(context, onCodeGen["number"], [
				getNumber(value),
				getString(`${value}`)
			], "builtin", true, null, dest, null);
		}
	},
	
	string(dest: CodeGenText, context: BuilderContext, value: string) {
		if (onCodeGen["string"]) {
			callFunction(context, onCodeGen["string"], [
				getString(value)
			], "builtin", true, null, dest, null);
		}
	},
	
	operator(dest: CodeGenText, context: BuilderContext, operatorText: string, leftText: string[], rightText: string[]) {
		if (onCodeGen["operator"]) {
			callFunction(context, onCodeGen["operator"], [
				getString(operatorText),
				getString(leftText.join("")),
				getString(rightText.join(""))
			], "builtin", true, null, dest, null);
		}
	},
	
	alias(dest: CodeGenText, context: BuilderContext, alias: ScopeObject, valueText: CodeGenText) {
		if (alias.kind == "alias" && alias.name && valueText) {
			if (onCodeGen["alias"]) {
				callFunction(context, onCodeGen["alias"], [
					getString(alias.symbolName),
					getString(valueText.join(""))
				], "builtin", true, null, dest, null);
			}
		}
	},
	
	load(dest: CodeGenText, context: BuilderContext, alias: ScopeObject) {
		if (alias.kind == "alias" && alias.name) {
			if (onCodeGen["identifier"]) {
				callFunction(context, onCodeGen["identifier"], [
					getString(alias.symbolName)
				], "builtin", true, null, dest, null);
			}
		}
	},
	
	set(dest: CodeGenText, context: BuilderContext, alias: ScopeObject, aliasText: CodeGenText, valueText: CodeGenText) {
		if (alias.kind == "alias" && alias.name && aliasText && valueText) {
			if (onCodeGen["set_alias"]) {
				callFunction(context, onCodeGen["set_alias"], [
					getString(alias.symbolName),
					getString(aliasText.join("")),
					getString(valueText.join(""))
				], "builtin", true, null, dest, null);
			}
		}
	},
	
	memberAccess(dest: CodeGenText, context: BuilderContext, leftText: string, name: string) {
		if (leftText) {
			if (onCodeGen["memberAccess"]) {
				callFunction(context, onCodeGen["memberAccess"], [
					getString(leftText),
					getString(name),
				], "builtin", true, null, dest, null);
			}
		}
	},
	
	struct(dest: CodeGenText, context: BuilderContext, struct: ScopeObject, codeBlockText: CodeGenText) {
		if ((struct.kind == "struct" || struct.kind == "structInstance") && codeBlockText) {
			if (onCodeGen["struct"]) {
				callFunction(context, onCodeGen["struct"], [
					getString(codeBlockText.join("")),
				], "builtin", true, null, dest, null);
			}
		}
	},
	
	if(dest: CodeGenText, context: BuilderContext, conditionText: CodeGenText, trueText: CodeGenText, falseText: CodeGenText) {
		if (conditionText && trueText && falseText) {
			if (onCodeGen["if"]) {
				callFunction(context, onCodeGen["if"], [
					getString(conditionText.join("")),
					getString(trueText.join("")),
					getString(falseText.join(""))
				], "builtin", true, null, dest, null);
			}
		}
	},
	
	function(dest: CodeGenText, context: BuilderContext, fn: ScopeObject, codeBlockText: CodeGenText) {
		if (fn.kind == "function" && codeBlockText) {
			if (onCodeGen["fn_arg"] && onCodeGen["fn"]) {
				const argDest = getCGText();
				for (let i = 0; i < fn.functionArguments.length; i++) {
					const argument = fn.functionArguments[i];
					if (argument.kind == "argument") {
						callFunction(context, onCodeGen["fn_arg"], [getString(argument.name), unwrapScopeObject(argument.type), getBool(fn.functionArguments[i+1] != undefined)], "builtin", true, null, argDest, null);
					} else {
						utilities.unreachable();
					}
				}
				callFunction(context, onCodeGen["fn"], [getString(fn.symbolName), getString(argDest.join("")), getString(codeBlockText.join(""))], "builtin", true, null, dest, null);
			}
		}
	},
	
	return(dest: CodeGenText, context: BuilderContext, valueText: CodeGenText) {
		if (valueText && context.file.scope.generatingFunction) {
			if (onCodeGen["return"]) {
				callFunction(context, onCodeGen["return"], [
					getString(valueText.join(""))
				], "builtin", true, null, dest, null);
			}
		}
	},
	
	call(dest: CodeGenText, context: BuilderContext, fn: ScopeObject, argumentText: CodeGenText) {
		if (dest && fn.kind == "function" && dest && argumentText && context.file.scope.generatingFunction) {
			if (onCodeGen["call_arg"] && onCodeGen["call"]) {
				const argDest = getCGText();
				for (let i = 0; i < argumentText.length; i++) {
					callFunction(context, onCodeGen["call_arg"], [
						getString(argumentText[i]),
						getBool(argumentText[i+1] != undefined)
					], "builtin", true, null, argDest, null);
				}
				callFunction(context, onCodeGen["call"], [
					getString(fn.symbolName),
					getBool(fn.external),
					getString(argDest.join(""))
				], "builtin", true, null, dest, null);
			}
		}
	},
	
	panic(dest: CodeGenText, context: BuilderContext, text: string) {
		if (onCodeGen["panic"]) {
			callFunction(context, onCodeGen["panic"], [
				getString(text),
			], "builtin", true, null, dest, null);
		}
	}
}