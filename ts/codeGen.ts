import {
	CodeGenText,
	getCGText,
	unwrapScopeObject,
} from "./types";
import { BuilderContext, callFunction } from "./builder";
import { ScopeObject } from "./types";
import { getBool, getNumber, getString, onCodeGen } from "./builtin";
import utilities from "./utilities";

export default {
	start(context: BuilderContext) {
		if (onCodeGen["start"]) {
			callFunction(context, onCodeGen["start"], [], "builtin", true, null, context.topCodeGenText, null);
		}
	},
	
	bool(dest: CodeGenText, context: BuilderContext, value: boolean) {
		if (onCodeGen["bool"]) {
			callFunction(context, onCodeGen["bool"], [getBool(value)], "builtin", true, null, dest, null);
		}
	},
	
	number(dest: CodeGenText, context: BuilderContext, value: number) {
		if (onCodeGen["number"]) {
			callFunction(context, onCodeGen["number"], [getNumber(value), getString(`${value}`)], "builtin", true, null, dest, null);
		}
	},
	
	string(dest: CodeGenText, context: BuilderContext, value: string) {
		if (onCodeGen["string"]) {
			callFunction(context, onCodeGen["string"], [getString(value)], "builtin", true, null, dest, null);
		}
	},
	
	alias(dest: CodeGenText, context: BuilderContext, alias: ScopeObject, valueText: CodeGenText) {
		if (alias.kind == "alias" && alias.name && valueText) {
			if (onCodeGen["alias"]) {
				callFunction(context, onCodeGen["alias"], [getString(alias.symbolName), getString(valueText.join("")), getBool(alias.mutable)], "builtin", true, null, dest, null);
			}
		}
	},
	
	load(dest: CodeGenText, context: BuilderContext, alias: ScopeObject) {
		if (alias.kind == "alias" && alias.name) {
			if (onCodeGen["identifier"]) {
				callFunction(context, onCodeGen["identifier"], [getString(alias.symbolName)], "builtin", true, null, dest, null);
			}
		}
	},
	
	set(dest: CodeGenText, context: BuilderContext, alias: ScopeObject, aliasText: CodeGenText, valueText: CodeGenText) {
		if (alias.kind == "alias" && alias.name && aliasText && valueText) {
			if (onCodeGen["set_alias"]) {
				callFunction(context, onCodeGen["set_alias"], [getString(alias.symbolName), getString(aliasText.join("")), getString(valueText.join(""))], "builtin", true, null, dest, null);
			}
		}
	},
	
	if(dest: CodeGenText, context: BuilderContext, conditionText: CodeGenText, trueText: CodeGenText, falseText: CodeGenText) {
		if (conditionText && trueText && falseText) {
			if (onCodeGen["if"]) {
				callFunction(context, onCodeGen["if"], [getString(conditionText.join("")), getString(trueText.join("")), getString(falseText.join(""))], "builtin", true, null, dest, null);
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
	
	call(dest: CodeGenText, context: BuilderContext, fn: ScopeObject, argumentText: CodeGenText) {
		if (dest && fn.kind == "function" && dest && argumentText) {
			if (onCodeGen["call_arg"] && onCodeGen["call"]) {
				const argDest = getCGText();
				for (let i = 0; i < argumentText.length; i++) {
					callFunction(context, onCodeGen["call_arg"], [getString(argumentText[i]), getBool(argumentText[i+1] != undefined)], "builtin", true, null, argDest, null);
				}
				callFunction(context, onCodeGen["call"], [getString(fn.symbolName), getString(argDest.join(""))], "builtin", true, null, dest, null);
			}
		}
	},
}