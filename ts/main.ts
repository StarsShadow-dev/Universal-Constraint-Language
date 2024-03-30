import { compileFile } from "./compiler";

(async () => {
	try {
		await compileFile("test/main.llcl");	
	} catch (error) {
		if (error == "__do nothing__") {
			return;
		}
		throw error;
	}
})();