const char INDEX_HTML[] PROGMEM = R"=====(
<!DOCTYPE html><html ng-app=myApp ng-cloak lang=en><meta charset=utf-8><link rel=icon href=https://image.flaticon.com/icons/png/128/115/115976.png><meta name=apple-mobile-web-app-capable content=yes><meta name=viewport content="width=device-width,initial-scale=1"><link rel=stylesheet href=https://ajax.googleapis.com/ajax/libs/angular_material/1.1.0/angular-material.min.css><link href="https://fonts.googleapis.com/icon?family=Material+Icons" rel=stylesheet><style>.inactive{background:#d1d1d1;opacity:.5}md-toolbar i{vertical-align:middle!important;display:inline-block;padding-bottom:3px}</style><body layout=column style=overflow-y:hidden><div ng-controller=dash style=overflow-y:auto md-theme=custom ng-mouseup=considerUpdate()><md-toolbar><div class=md-toolbar-tools><h2 flex md-truncate><i ng-if=isRemote() class=material-icons>cast</i> {{dash.app_name}}</h2><md-button class=md-icon-button><i class=material-icons ng-style=getPowerStyle() ng-click="doAction('toggle')">power_settings_new</i></md-button></div></md-toolbar><md-progress-linear ng-if=network.ips_to_check.length md-mode=determinate class=md-warn value="{{ network.ip_scan_percentage }} "></md-progress-linear><md-tabs md-dynamic-height md-border-bottom md-selected=selected_tab_index><md-tab label=Devices data-if="tpl.devices.length || network.devices.length"><md-content><md-list class=md-dense flex><md-subheader ng-if=network.devices.length>ESP8266 Devices<md-icon class=material-icons ng-click=refresh_network_devices()>autorenew</md-icon></md-subheader><md-list-item class=md-2-line ng-repeat="loc_device in network.devices track by $index"><img src=https://pbs.twimg.com/profile_images/928655749605199872/ygogGX2j.jpg class=md-avatar ng-click=load_network_device_into_dash($index) alt=""><div class=md-list-item-text><h3>{{loc_device.app_name}}</h3><p>{{loc_device.app_desc}} - [{{loc_device.address}}]</div><div ng-show="loc_device.mode == 'percentage'" class=md-list-item-text style="padding:0 8px"><md-slider flex min=0 max=100 ng-model=loc_device.percentage ng-click="remoteRequest(loc_device.address + '/' + loc_device.request.base_url + '?' + loc_device.request.start_param + '=' + loc_device.percentage)" aria-label={{dash.perc_label}}></md-slider></div><md-switch ng-if="loc_device.mode=='toggle'" ng-change="remoteRequest(loc_device.address + '/' + loc_device.request.base_url + '?' + loc_device.request.master_param + '=' + (loc_device.is_powered ? 'true' : 'false'))" ng-model=loc_device.is_powered></md-switch><md-button ng-if="loc_device.mode=='momentary'" class="md-raised md-primary" ng-click="remoteRequest(loc_device.address + '/' + loc_device.request.base_url + '?' + loc_device.request.master_param + '=true')">trigger</md-button><md-divider></md-divider></md-list-item><md-subheader ng-if=tpl.devices.length>TP-Link Devices<md-icon class=material-icons ng-click=tpl_refreshDevices()>autorenew</md-icon></md-subheader><md-list-item class=md-2-line ng-repeat="device in tpl.devices"><img src=https://cdn-01.independent.ie/business/technology/reviews/article35225129.ece/12d93/AUTOCROP/h342/2016-11-19_bus_26361599_I1.JPG class=md-avatar alt=""><div class=md-list-item-text><h3>{{device.alias}}</h3><p>{{device.deviceModel}} - {{device.deviceName}}</div><md-switch ng-change=tpl_setState($index,device.is_powered) ng-model=device.is_powered></md-switch><md-divider></md-divider></md-list-item></md-list></md-content></md-tab><md-tab label=Detail><md-content><md-list><md-list-item class=md-2-line ng-show="dash.mode != 'percentage'"><div class=md-list-item-text><h3>Power</h3><p>Turn the {{dash.app_name}} {{ dash.is_powered ? 'off' : 'on' }}</div><md-switch ng-show="dash.mode=='toggle'" ng-change="doAction('master')" ng-model=dash.is_powered></md-switch><md-button ng-show="dash.mode=='momentary'" ng-click="doAction('master')" class="md-raised md-primary">trigger</md-button><md-divider></md-divider></md-list-item><md-list-item class=md-2-line ng-show="dash.mode == 'percentage'"><div class=md-list-item-text><h3>{{dash.perc_label}}: {{dash.percentage}}</h3></div><div class=md-list-item-text style="padding:0 8px"><md-slider flex min=0 max=100 ng-model=dash.percentage ng-click="doAction('percentage')" aria-label={{dash.perc_label}}></md-slider></div><md-divider></md-divider></md-list-item><md-list-item class=md-2-line ng-show=dash.request.duration_param><div class=md-list-item-text><h3>Duration<span hide show-gt-xs> of transition</span>: <span ng-show="duration == 0">Immediate</span> <span ng-show="duration != 0">{{duration}} mins.</span></h3></div><div class=md-list-item-text style="padding:0 8px"><md-slider flex min=0 max=120 ng-model=duration aria-label=duration></md-slider></div><md-divider></md-divider></md-list-item><md-list-item class=md-2-line><div class=md-list-item-text><h3>Timer</h3><p>{{ dash.is_using_timer ? (dash.is_skipping_next ? 'Skipping ' : 'Enacting ') + dash.next_event_name + ' in ' + dash.next_event_due + ' mins' : 'Turn timer on' }}</div><md-switch ng-change="doAction('timer')" ng-model=dash.is_using_timer></md-switch><md-divider></md-divider></md-list-item><md-list-item class=md-2-line ng-class="{ inactive: !dash.is_using_timer }"><div class=md-list-item-text><h3>Skip Next</h3><p>{{ dash.is_using_timer ? (dash.is_skipping_next ? 'Next scheduled event will be skipped' : 'Override the next scheduled event') : 'Requires timer mode' }}</div><md-switch ng-disabled=!dash.is_using_timer ng-change="doAction('skip')" ng-model=dash.is_skipping_next></md-switch><md-divider></md-divider></md-list-item><md-list class=md-dense ng-class="{ inactive: !dash.is_using_timer }" layout=column layout-wrap layout-gt-sm=row><md-list-item flex=50 class=md-3-line ng-repeat="event in dash.events"><div class=md-avatar><i ng-if=!event.enacted class=material-icons style=font-size:48px>schedule</i> <i ng-if=event.enacted class=material-icons style=font-size:48px>check_circle</i></div><div class=md-list-item-text><h3>{{event.time}}</h3><h4>Enacting {{event.label}}</h4><p ng-if=event.enacted>Performed today<p ng-if=!event.enacted>Not yet performed today</div></md-list-item></md-list><md-divider></md-divider></md-list></md-content></md-tab><md-tab label=Settings><div layout=column layout-gt-sm=row><div flex=50><md-card><md-content class=md-padding><h3>Local ESP8266 Devices</h3><p class=md-caption>Search for local devices for the following IP range.<div layout=column layout-gt-xs=row><md-input-container flex=50><label>Start at: {{ get123() }}<strong>{{network.scan.ip_range_start}}</strong></label><input required type=number ng-model=network.scan.ip_range_start min=1 max=255><div ng-messages=network.scan.ip_range_start.$error role=alert><div ng-message-exp="['required', 'min', 'max']">IP Range must be between 1 and 255.</div></div></md-input-container><md-input-container flex=50><label>End at: {{ get123() }}<strong>{{network.scan.ip_range_end}}</strong></label><input required type=number ng-model=network.scan.ip_range_end min=1 max=255><div ng-messages=network.scan.ip_range_end.$error role=alert><div ng-message-exp="['required', 'min', 'max']">IP Range must be between 1 and 255.</div></div></md-input-container></div><div layout=column layout-gt-xs=row><md-input-container flex=50><md-checkbox ng-model=loc.store_detected aria-label="store detected">Allow this browser to store addresses of detected devices.</md-checkbox></md-input-container><md-input-container flex=50><md-checkbox ng-model=loc.replace_detected aria-label="replace detected">Replace existing local list with these results.</md-checkbox></md-input-container></div><md-button class="md-button md-raised md-primary" ng-click=detect_network_devices()>Detect Local Devices</md-button><div><br><md-divider></md-divider><div ng-if="network.ip_scan_percentage > 0 && network.ip_scan_percentage < 100"><h3>Scanning {{network.ip_scan_percentage}}%</h3><md-list class=md-dense flex><md-subheader class=md-no-sticky>Local Device Queue</md-subheader><md-list-item class=md-2-line ng-repeat="ip in network.ips_to_check"><div class=md-list-item-text><h3>{{ip.ip_address}}</h3><p>{{ (ip.checked ? "Request Sent" : "Pending") }} - {{ip.result}}<md-progress-linear ng-if="ip.result == 'waiting for response...'" md-mode=indeterminate></md-progress-linear></div><md-divider></md-divider></md-list-item></md-list></div><h3>Current Status</h3><p class=md-caption>Found {{network.devices.length}} devices.</div></md-content></md-card></div><div flex=50><md-card><md-content class=md-padding><h3>TP-Link Authentication</h3><p class=md-caption>Please provide the credentials used to connect to your TP-Link account.<div layout=column layout-gt-xs=row><md-input-container flex=50><label>Username/Email</label><input required type=email ng-model=tpl.creds.username minlength=10 maxlength=100 ng-pattern=/^.+@.+\..+$/ ><div ng-messages=tpl.creds.username.$error role=alert><div ng-message-exp="['required', 'minlength', 'maxlength', 'pattern']">Your email must be between 10 and 100 characters long and look like an e-mail address.</div></div></md-input-container><md-input-container flex=50><label>Password</label><input required ng-model=tpl.creds.password><div ng-messages=tpl.password.$error role=alert><div ng-message-exp="['required']">Password must be provided.</div></div></md-input-container></div><div layout=column layout-gt-xs=row><md-input-container flex=50><md-checkbox ng-model=tpl.creds.store aria-label="store credentials">Allow this browser to store credentials.</md-checkbox></md-input-container><md-input-container flex=50><md-checkbox ng-model=tpl.token.store aria-label="store token">Allow this browser to store the authentication token.</md-checkbox></md-input-container></div><md-button class="md-button md-raised md-primary" ng-click=tpl_authenticate()>Authenticate and Refresh Devices</md-button><div ng-if="tpl.token.value != ''"><br><md-divider></md-divider><h3>Current Status</h3><p class=md-caption>Found {{tpl.devices.length}} devices using token: <code>{{tpl.token.value}}</code></div></md-content></md-card></div></div></md-tab></md-tabs><md-content class="md-padding md-caption"><p>{{dash.app_name}} <small>{{dash.app_version}}.<br>Last sync: {{dash.time_of_day}}<br>Last action: {{dash.last_action}}<br><a href=/update>Update Software</a></small></p></md-content></div><script src=https://ajax.googleapis.com/ajax/libs/angularjs//1.6.4/angular.min.js></script><script src=https://ajax.googleapis.com/ajax/libs/angularjs/1.6.4/angular-animate.min.js></script><script src=https://ajax.googleapis.com/ajax/libs/angularjs/1.6.4/angular-aria.min.js></script><script src=https://ajax.googleapis.com/ajax/libs/angularjs/1.6.4/angular-messages.min.js></script><script src=https://ajax.googleapis.com/ajax/libs/angular_material/1.1.4/angular-material.min.js></script><script src=script.js></script>
)=====";
