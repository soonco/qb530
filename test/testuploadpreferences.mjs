// Run with: node test/testuploadpreferences.mjs
// Exercise the actual embedded preferences script without starting a WebUI server.
import assert from "node:assert/strict";
import fs from "node:fs";
import vm from "node:vm";

const html = fs.readFileSync(new URL("../src/webui/www/private/views/preferences.html", import.meta.url), "utf8");
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const elements = new Map();
const document = {
    getElementById(id) {
        if (!elements.has(id))
            elements.set(id, { value: "", checked: false, disabled: false, addEventListener() {} });
        return elements.get(id);
    },
};
const window = {};
// setup() needs the rest of the application; the module and handler run unchanged.
vm.runInNewContext(script.replace("window.qBittorrent.Preferences.setup();", ""), { window, document });
const ratio = document.getElementById("fakeUploadRatio");
const random = document.getElementById("fakeUploadRatioRandomization");
const update = window.qBittorrent.Preferences.updateFakeUploadRatioRandomizationEnabled;
ratio.value = "1";
random.checked = true;
update();
assert.equal(random.disabled, true);
assert.equal(random.checked, false);
ratio.value = "5";
update();
assert.equal(random.disabled, false);
assert.equal(random.checked, false);
random.checked = true;
ratio.value = "1";
update();
ratio.value = "5";
update();
assert.equal(random.checked, false);
assert.equal(random.disabled, false);
console.log("Upload preferences: 1x/5x enable, clear, and no implicit restore passed.");
