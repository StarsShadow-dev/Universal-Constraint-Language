import crypto from "crypto";

import { ASTnode } from "./parser";
import logger, { LogType } from "./logger";
import utilities from "./utilities";
import { evaluate } from "./evaluate";
import { printASTnode } from "./printAST";
import { Indicator } from "./report";

type _topLevelDef = {
	// uuid: string,
	name: string,
};
export type topLevelDef = _topLevelDef & {
	ASTnode: ASTnode,
};
export type topLevelDef_orNull = _topLevelDef & {
	ASTnode: ASTnode | null,
};

export type DB = {
	defs: topLevelDef_orNull[],
	// changeLog
	topLevelEvaluations: Indicator[],
};
export function makeDB(): DB {
	return {
		defs: [],
		topLevelEvaluations: [],
	};
}

type Hash = string;
function hashString(text: string): Hash {
	return crypto.createHash("sha256").update(text).digest("hex");
}
function getUUID(): string {
	const byteSize = 10;
	const bytes = crypto.getRandomValues(new Uint8Array(byteSize));
	let string = "";
	for (let i = 0; i < bytes.length; i++) {
		const byteString = bytes[i].toString(16).toUpperCase();
		if (byteString.length == 1) {
			string += "0";
		}
		string += byteString;
	}
	return string;
}

export function getDefFromList_orNull(list: topLevelDef_orNull[], name: string): topLevelDef_orNull | null {
	for (let i = 0; i < list.length; i++) {
		const def = list[i];
		if (def.name == name) {
			return def as topLevelDef;
		}
	}

	return null;
}
export function getDefFromList(list: topLevelDef_orNull[], name: string): topLevelDef | null {
	for (let i = 0; i < list.length; i++) {
		const def = list[i];
		if (def.ASTnode == null) {
			throw utilities.unreachable();
		}
		if (def.name == name) {
			return def as topLevelDef;
		}
	}

	return null;
}

export type ResolveAliasesContext = {
	db: DB,
};

export function resolveAliases(context: ResolveAliasesContext, AST: ASTnode[]) {
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		switch (ASTnode.kind) {
			case "identifier": {
				ASTnode.name;
				const def = getDefFromList(context.db.defs, ASTnode.name);
				if (!def) {
					throw utilities.TODO();
				}
				ASTnode.name = def.name;
				
				break;
			}
			
			case "call": {
				resolveAliases(context, [ASTnode.left]);
				
				break;
			}
		}
	}
}

export function addToDB(db: DB, AST: ASTnode[]) {
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		
		if (ASTnode.kind == "alias") {
			// const hash = hashString(JSON.stringify(ASTnode.value)); // TODO: locations...
			const uuid = getUUID();
			db.defs.push({
				// uuid: uuid,
				name: ASTnode.name,
				ASTnode: null,
			});
			logger.log(LogType.addToDB, `added ${ASTnode.name} as ${uuid}`);
		}
	}
	
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		
		if (ASTnode.kind == "alias") {
			const def = getDefFromList_orNull(db.defs, ASTnode.name);
			if (!def) throw utilities.unreachable();
			const value = ASTnode.value;
			
			switch (value.kind) {
				case "function": {
					// TODO: resolveAliases()
					def.ASTnode = value;
					logger.log(LogType.addToDB, `added AST to ${def.name}`);
					
					break;
				}
			}
		} else {
			const location = ASTnode.location;
			resolveAliases({ db: db }, [ASTnode]);
			const result = evaluate({ db: db }, ASTnode);
			const resultText = printASTnode(result);
			
			db.topLevelEvaluations.push({ location: location, msg: `${resultText}` });
		}
	}
}