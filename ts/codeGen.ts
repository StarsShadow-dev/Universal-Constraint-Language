import { BuilderContext, callFunction } from "./builder";
import { getString, onCodeGen } from "./builtin";
import { ScopeObject } from "./types";

export default {
	getNewName(context: BuilderContext, name: string): string | null {
		let generatedName: string | null = null;
		if (context.options.doCodeGen) {
			generatedName = name;
		}
		
		return generatedName;
	},
	
	identifier(context: BuilderContext, alias: ScopeObject) {
		if (alias.kind == "alias" && alias.generatedName) {
			if (context.options.doCodeGen && onCodeGen["identifier"]) {
				callFunction(context, onCodeGen["identifier"], [getString(alias.generatedName)], "builtin", false, false);
			}	
		}
	}
}