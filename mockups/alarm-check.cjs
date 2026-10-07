const {chromium}=require('/Users/Scott/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/playwright');
const {pathToFileURL}=require('url');
const path=require('path');
const assert=require('assert');
(async()=>{
const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
try{
 const page=await browser.newPage({viewport:{width:900,height:1000}});const errors=[];page.on('pageerror',e=>errors.push(e.message));
 await page.goto(pathToFileURL(path.join(__dirname,'alarm-design.html')).href);await page.frameLocator('iframe').locator('#ap-content .ap-backdrop').waitFor({state:'attached'});
 const frame=page.frames().find(f=>f!==page.mainFrame());await frame.evaluate(()=>document.fonts.ready);
 await frame.evaluate(()=>window.dispatchEvent(new CustomEvent('openai:set_globals',{detail:{globals:{widgetState:{modelContent:{alarm:{enabled:true,hour:7,minute:0,days:[1,2,3,4,5],snooze:10,volume:40}}}}}})));
 await frame.locator('[data-preview="settings"]').click();
 assert.equal(await frame.getByRole('switch',{name:'Alarm',exact:true}).getAttribute('aria-checked'),'true');
 await frame.getByRole('switch',{name:'Alarm',exact:true}).click();assert.equal(await frame.getByRole('switch',{name:'Alarm',exact:true}).getAttribute('aria-checked'),'false');
 await frame.getByRole('switch',{name:'Alarm',exact:true}).click();assert.equal(await frame.getByRole('switch',{name:'Alarm',exact:true}).getAttribute('aria-checked'),'true');
 assert(!(await frame.locator('#ap-content').innerText()).includes('Enabled'));
 await frame.locator('#ap-content [data-action="time"]').click();await frame.locator('#ap-content [data-action="hour+"]').click();await frame.locator('#ap-content [data-action="editor-done"]').click();
 assert((await frame.locator('#ap-content').innerText()).includes('8:00 AM'));
 await frame.locator('#ap-content [data-action="sound"]').click();await frame.locator('#ap-content [data-action="snooze"]').click();await frame.locator('#ap-content [data-action="duration-5"]').click();await frame.locator('#ap-content [data-action="snooze-done"]').click();
 await frame.locator('.ap-range').fill('55');await frame.locator('#ap-content [data-action="editor-done"]').click();await frame.locator('#ap-content [data-action="save"]').click();assert((await frame.locator('#ap-content').innerText()).includes('8:00 AM'));
 await frame.locator('[data-preview="ringing"]').click();assert((await frame.locator('[data-ring-layout="digital"]').innerText()).includes('MONDAY'));await frame.locator('[data-ring-layout="digital"] [data-action="ring-snooze"]').click();assert((await frame.locator('#ap-content').innerText()).includes('5:00'));assert((await frame.locator('#ap-content').innerText()).includes('8:05 AM'));assert((await frame.locator('#ap-content .ap-digital-time').innerText()).includes('8:00'));
 await frame.locator('#ap-content [data-action="ring-dismiss"]').click();assert((await frame.locator('#ap-content').innerText()).includes('8:00 AM'));
 await frame.locator('[data-preview="settings"]').click();await frame.locator('#ap-content [data-action="time"]').click();await frame.locator('#ap-content [data-action="hour+"]').click();await frame.locator('#ap-content [data-action="editor-done"]').click();await frame.locator('#ap-content [data-action="cancel"]').click();assert((await frame.locator('#ap-content').innerText()).includes('8:00 AM'));
 await frame.locator('#alarm-time-valid').uncheck();assert((await frame.locator('#ap-content').innerText()).includes('--:--'));assert(await frame.locator('[data-preview="ringing"]').isDisabled());await frame.locator('#alarm-time-valid').check();
 for(const width of [320,736]){await page.setViewportSize({width:width+32,height:1000});await frame.locator('[data-preview="settings"]').click();let overflow=await frame.evaluate(()=>document.documentElement.scrollWidth>document.documentElement.clientWidth);assert(!overflow,`Overflow at ${width}`)}
 // Ordinary long settings must remain readable inside their buttons.
 await frame.evaluate(()=>window.dispatchEvent(new CustomEvent('openai:set_globals',{detail:{globals:{widgetState:{modelContent:{alarm:{enabled:true,hour:12,minute:59,days:[0,1,2,3,4,5],snooze:10,volume:40}}}}}})));
 await frame.locator('[data-preview="settings"]').click();
 assert((await frame.locator('#ap-content').innerText()).includes('12:59'));
 assert((await frame.locator('#ap-content').innerText()).includes('6 days'));
 const clipped=await frame.locator('#ap-content .ap-button').evaluateAll(els=>els.filter(el=>el.scrollWidth>el.clientWidth+2).map(el=>el.textContent));assert.deepEqual(clipped,[]);
 // Reset the sample before exporting the design views.
 await page.setViewportSize({width:1100,height:1100});await page.evaluate(()=>localStorage.clear());await page.reload();await page.frameLocator('iframe').locator('#ap-content .ap-backdrop').waitFor({state:'attached'});
 const f=page.frames().find(f=>f!==page.mainFrame());await f.evaluate(()=>document.fonts.ready);
 await f.evaluate(()=>window.dispatchEvent(new CustomEvent('openai:set_globals',{detail:{globals:{widgetState:{modelContent:{alarm:{enabled:true,hour:7,minute:0,days:[1,2,3,4,5],snooze:10,volume:40}}}}}})));
 await f.evaluate(()=>document.querySelectorAll('.ap-frame').forEach(el=>el.style.width='800px'));
 for(const view of ['digital','analog','device','settings','ringing','snoozed']){
  await f.locator(`[data-preview="${view}"]`).click();const target=view==='ringing'?f.locator('[data-ring-layout="digital"]'):f.locator('#ap-content');await target.screenshot({path:path.join(__dirname,'screens','alarm-'+view+'.png')});
 }
 await f.locator('[data-preview="settings"]').click();
 for(const view of ['time','repeat','sound']){await f.locator(`#ap-content [data-action="${view}"]`).click();await f.locator('#ap-content').screenshot({path:path.join(__dirname,'screens','alarm-'+view+'.png')});await f.locator('#ap-content [data-action="editor-done"]').click()}
 await f.locator('#ap-content [data-action="sound"]').click();await f.locator('#ap-content [data-action="snooze"]').click();await f.locator('#ap-content').screenshot({path:path.join(__dirname,'screens','alarm-snooze-setting.png')});
 await f.locator('#alarm-night').check();await f.locator('[data-preview="ringing"]').click();await f.locator('[data-ring-layout="digital"]').screenshot({path:path.join(__dirname,'screens','alarm-ringing-night.png')});
 await f.locator('[data-preview="snoozed"]').click();await f.locator('#ap-content').screenshot({path:path.join(__dirname,'screens','alarm-snoozed-night.png')});
 assert.deepEqual(errors,[]);console.log('Passed: Alarm switch; time, snooze and volume edits; save/cancel; digital ringing and snooze/dismiss; no-time state; narrow layout; screen exports.');
}finally{await browser.close()}
})();
