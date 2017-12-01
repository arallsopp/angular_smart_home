'use strict';

var app = angular.module('myApp', ['ngMaterial']);

myDash.$inject = ['$scope', '$mdToast','$http','$interval','$sce','$timeout'];

angular.module('myApp').controller('dash', myDash)
    .config(['$mdThemingProvider',function($mdThemingProvider) {
        $mdThemingProvider.theme('custom').primaryPalette('blue-grey').accentPalette('deep-orange');
    }]);

function myDash($scope, $mdToast, $http, $interval, $sce,$timeout) {

    $scope.time_of_day = '--';
    $scope.next_event_due = '--';
    $scope.app_name = '--';
    $scope.app_version = '--';

    $scope.is_dst = false;
    $scope.is_skipping_next = false;
    $scope.is_powered = false;
    $scope.is_using_timer = false;
    $scope.percentage = 0;
    $scope.request = null;
    $scope.perc_label = '--';

    $scope.last_action = '--';
    $scope.events = [];

    $scope.doAction = function (mode) {
        var url = $scope.request.base_url + "?";
        var param = false;

        switch (mode) {
            case 'percentage':
                $scope.is_powered = ($scope.percentage > 0);
                param = $scope.request.start_param + "=" + $scope.percentage ;
                break;

            case 'toggle':
                $scope.is_powered = !$scope.is_powered; /* flow through */
                if($scope.supports_percentage){
                    if(!$scope.is_powered){
                        $scope.percentage = 0;
                    }else if ($scope.percentage == 0) {
                        $scope.percentage = 1;
                    }
                    param = $scope.request.start_param + "=" + $scope.percentage ; /* make sure this gets sent instead of power */
                    break;
                }else{
                    /* do not break, allow to flow through */
                }

            case 'master':
                param = $scope.request.master_param + '=' + ($scope.is_powered ? 'true' : 'false');
                break;
            case 'timer':
                param = 'timer=' + ($scope.is_using_timer ? 'true' : 'false');
                break;
            case 'skip':
                param = 'skip=' + ($scope.is_skipping_next ? 'true' : 'false');
                break;
            default :
                alert('did not understand')

        }
        $http.get(url + param).then(function (response) {
            $scope.showToast(response.data.message);
        });
    };

    $scope.showToast = function (msg) {
        $mdToast.show($mdToast.simple().textContent(msg).position('top right'))
    };

    $scope.loc_getStatus = function () {
        $http.get('status.php').then(function (response) {
            console.log('received', response.data);
            $scope.updateUI(response.data);
        });
    };
    $scope.loc_refreshDevices = function(){
        console.log('do it');

        $scope.loc.ips_to_check = [];
        $scope.loc.ips_counted = 0;
        $scope.loc.ips_checked = 0;
        $scope.loc.ip_scan_percentage = 0;

        for (var i = 0; i <  $scope.loc.devices.length; i++) {
            $scope.loc.ips_to_check.push({"ip_address": "http://" + $scope.loc.devices[i].address,"checked":false,"result":"Held in queue"});
        }

        $scope.loc_try_next_in_list();

    };

    $scope.getPowerStyle = function(){
        if(!$scope.is_powered){

            return {'color': 'rgba(255, 255, 255, 0.3)'};
        }
    };

    $scope.updateUI = function (data) { /* from status.php */
        $scope.time_of_day = data.time_of_day;
        $scope.last_action = data.last_action;
        $scope.app_name = data.app_name;
        $scope.is_dst = data.is_dst;
        $scope.is_using_timer = data.is_using_timer;
        $scope.is_powered = data.is_powered;
        $scope.supports_percentage = (data.percentage !== undefined);
        $scope.percentage = data.percentage;
        $scope.perc_label = data.perc_label;
        $scope.is_skipping_next = data.is_skipping_next;
        $scope.next_event_due = data.next_event_due;
        $scope.app_version = data.app_version;
        $scope.events = data.events;
        $scope.request = data.request;
        document.title = data.app_name;

        $scope.showToast('Synchronised');
    };

    $scope.loc_getStatus();
    $interval(function () {
        $scope.loc_getStatus();
    }, 60 * 1000);

    $scope.loc_detect_devices = function() {

        localStorage.setItem('loc.scan',JSON.stringify($scope.loc.scan));

        $scope.loc.ips_to_check = [];
        $scope.loc.ips_counted = 0;

        if($scope.loc.replace_detected){
            $scope.loc.devices = [];
        }

        var workitout = "http://192.168.0."; //todo: this should come from status.

        for (var i = $scope.loc.scan.ip_range_start; i <= $scope.loc.scan.ip_range_end; i++) {
            $scope.loc.ips_to_check.push({"ip_address": workitout + i,"checked":false,"result":"Held in queue"});
        }

        $scope.loc.ips_checked = 0;
        $scope.loc.ip_scan_percentage = 0;

        $timeout(function () {
            $scope.loc_try_next_in_list();
        }, 2 * 1000);

        $scope.loc_doStore();
    };

    $scope.loc_device_merged_in=function(data){
        console.log('checking',data);
        var newaddress = (data.address);
        for(var i=0;i<$scope.loc.devices.length;i++){
            if ($scope.loc.devices[i].address == data.address){
                $scope.loc.devices[i] = data;
                console.log('merged');
                return true;
            }
        }
        return false;
    };

    $scope.loc_try_next_in_list = function(){
        for(var i=0;i<$scope.loc.ips_to_check.length;i++){
            if($scope.loc.ips_to_check[i].checked){
                //done that one.
            }else{
                console.log('kicking off a test for ', $scope.loc.ips_to_check[i]);
                var url = $scope.loc.ips_to_check[i].ip_address + "/features.json";
                var trustedUrl = $sce.trustAsResourceUrl(url);
                let index = i;
                $scope.loc.ips_to_check[index].result = 'waiting for response...';

                $http.jsonp(trustedUrl, {jsonpCallbackParam: 'callback',timeout:10000}).then(function onSuccess(data) {
                    console.log('got something!', data.data);
                    $scope.loc.ips_to_check[index].result = 'Found device ' + data.data.app_name;
                    //check that the device isn't in the list.

                    if($scope.loc_device_merged_in(data.data)) {
                        $scope.loc.ips_to_check[index].result = 'Updated device ' + data.data.app_name;
                    }else {
                        $scope.loc.devices.push(data.data);
                        console.log('added it');
                    }

                    if ($scope.loc.store_detected) {
                        $scope.loc_doStore();
                    }

                }).catch(function onError(response) {
                    console.log('checked off:',index);
                    $scope.loc.ips_to_check[index].result = 'No response';
                });

                $scope.loc.ips_to_check[i].checked = true;
                $scope.loc.ips_checked++;
                $scope.loc.ip_scan_percentage = parseInt(($scope.loc.ips_checked / $scope.loc.ips_to_check.length) * 100);

                $timeout(function () {
                    $scope.loc_try_next_in_list();
                }, 2 * 1000);
                return;
            }
        }
        console.log('all sites done.');
        $scope.loc.ips_to_check = [];
    };

    $scope.remoteRequest = function(address,on_url,off_url,is_powered){
        var url= 'http://' + address + '/' + (is_powered ? off_url : on_url);
        console.log('requesting: ' + url);
        var trustedUrl = $sce.trustAsResourceUrl(url);

        $http.jsonp(trustedUrl, {jsonpCallbackParam: 'callback'})
            .then(function onSuccess(data) {
                console.log(data.data);
                $scope.showToast(data.data.message);
            }).catch(function onError(response) {
            });
    };

    /* below this for tp link */
    $scope.getUUID = function () {
        var d = new Date().getTime();
        if (window.performance && typeof window.performance.now === "function") {
            d += performance.now();
            //use high-precision timer if available
        }
        var uuid = 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function (c) {
            var r = (d + Math.random() * 16) % 16 | 0;
            d = Math.floor(d / 16);
            return (c == 'x' ? r : (r & 0x3 | 0x8)).toString(16);
        });
        return uuid;
    };

    $scope.loc_doStore = function(){
        localStorage.setItem('loc.devices',JSON.stringify($scope.loc.devices));
    };

    $scope.tpl_authenticate = function () {
        $scope.tpl.UUID = $scope.getUUID();

        var auth_obj = {
            "method": "login", "params": {
                "appType": "Kasa_Android",
                "cloudUserName": $scope.tpl.creds.username,
                "cloudPassword": $scope.tpl.creds.password,
                "terminalUUID": $scope.tpl.UUID
            }
        };

        $http.post("https://wap.tplinkcloud.com/", auth_obj).

        then(function mySuccess(response) {

                if(response.data.error_code){
                    alert(response.data.msg);
                }else {
                    $scope.tpl.token.value = response.data.result.token;
                    localStorage.setItem('tpl_uuid', $scope.tpl.UUID);

                    if ($scope.tpl.creds.store) {
                        localStorage.setItem('tpl.creds', JSON.stringify($scope.tpl.creds));
                    } else {
                        localStorage.removeItem('tpl.creds');
                    }

                    if ($scope.tpl.token.store) {
                        localStorage.setItem('tpl.token', JSON.stringify($scope.tpl.token));
                    } else {
                        localStorage.removeItem('tpl.token');
                    }

                    $scope.tpl_refreshDevices();
                }
            }, function myError(response) {
                $scope.myWelcome = response.statusText;
            });

    };

    $scope.tpl_refreshDevices = function () {
        var request_obj = {"method": "getDeviceList"};

        $http.post("https://wap.tplinkcloud.com?token=" + $scope.tpl.token.value, request_obj).then(function mySuccess(response) {
            $scope.tpl.devices = (response.data.result.deviceList);
            console.log($scope.tpl.devices);
            if ($scope.tpl.devices.length) {
                for (var i = 0; i < $scope.tpl.devices.length; i++) {
                    $scope.tpl_getState(i);
                }

                $scope.selected_tab_index = 0;
            }
        });

    };

    $scope.tpl_getState = function (device_index) {
        var url = $scope.tpl.devices[device_index].appServerUrl;
        var deviceId = $scope.tpl.devices[device_index].deviceId;

        var request_obj = {
            "method": "passthrough", "params": {
                "deviceId": deviceId,
                "requestData": "{\"system\":{\"get_sysinfo\":null},\"emeter\":{\"get_realtime\":null}}"
            }
        };
        $http.post(url + "?token=" + $scope.tpl.token.value, request_obj).then(function mySuccess(response) {
            window.response = response;
            var testval = JSON.parse(response.data.result.responseData).system.get_sysinfo.relay_state;
            console.log(device_index, testval, response);

            $scope.tpl.devices[device_index].is_powered = (testval == true);
        });
    };

    $scope.tpl_setState = function (device_index, device_state) {
        var url = $scope.tpl.devices[device_index].appServerUrl;
        var deviceId = $scope.tpl.devices[device_index].deviceId;

        var request_obj = {
            "method": "passthrough", "params": {
                "deviceId": deviceId,
                "requestData": "{\"system\":{\"set_relay_state\":{\"state\":" + (device_state ? 1 : 0 ) + "}}}"
            }
        };
        $http.post(url + "?token=" + $scope.tpl.token.value, request_obj).then(function mySuccess(response) {
            window.response = response;
            console.log(response);
        });
    };

    $scope.tpl = {};
    $scope.tpl.refresh_rate = 60; //check every 5 seconds.

    $scope.tpl.devices = [];
    $scope.tpl.creds = JSON.parse(localStorage.getItem('tpl.creds'));
    $scope.tpl.token = JSON.parse(localStorage.getItem('tpl.token'));

    if($scope.tpl.creds === null){
        $scope.tpl.creds = {};
        $scope.tpl.creds.username = '';
        $scope.tpl.creds.password = '';
        $scope.tpl.creds.store = false;
    }

    $scope.tpl.UUID = localStorage.getItem('tpl_uuid');
    if($scope.tpl.token === null){
        $scope.tpl.token = {};
        $scope.tpl.token.value = '';
        $scope.tpl.token.store = true;
    }else{
        $scope.tpl_refreshDevices();
    }

    $scope.loc = {};
    $scope.loc.store_detected = true;
    $scope.loc.devices = JSON.parse(localStorage.getItem('loc.devices'));
    if($scope.loc.devices == null){
        $scope.loc.devices=[];
    }

    $scope.loc.scan = JSON.parse(localStorage.getItem('loc.scan'));

    if($scope.loc.scan === null) {
        $scope.loc.scan = {};
        $scope.loc.scan.ip_range_start = 2;
        $scope.loc.scan.ip_range_end = 40;
    }

    $scope.selected_tab_index = 0;


    $interval(function () {
        for (var i = 0; i < $scope.tpl.devices.length; i++) {
            $scope.tpl_getState(i);
        }
    }, $scope.tpl.refresh_rate * 1000);

};

