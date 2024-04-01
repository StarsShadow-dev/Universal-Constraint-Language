import {
	ScopeObject,
} from "./types";

export let builtinScopeLevel: ScopeObject[] = [];

function addType(name: string) {
	builtinScopeLevel.push({
		kind: "alias",
		mutable: false,
		originLocation: "core",
		name: name,
		value: [
			{
				kind: "type",
				originLocation: "core",
				name: "core:" + name
			}
		]
	});
}

export function setUpBuiltin() {
	addType("Bool");
	addType("Number");
	addType("String");
}