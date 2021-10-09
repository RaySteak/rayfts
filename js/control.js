_("on_state").disabled = true;
var clone_tr = _("ref_tr").cloneNode(true);
var tbody = _("tbody");
var count = 0;
_("ref_tr").remove();

function add_device(device_name, mac) {
    tbody.appendChild(clone_tr.cloneNode(true));
    _("device_name").innerHTML = device_name;
    _("mac").innerHTML = mac;
    _("device_name").id = "device_name" + (++count);
    _("mac").id = "mac" + count;
    _("on_state").id = "on_state" + count;
    _("awaken").setAttribute("onClick", "javascript: awaken(" + count + ")");
    _("awaken").id = "awaken" + count;
}

function awaken(id) {
    $.ajax({
        xhr: function() {
            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function() {
                if (xhr.readyState == 2) {
                    xhr.responseType = "text";
                }
            };
            return xhr;
        },
        url: window.location.pathname + "/~awaken",
        type: "PATCH",
        data: "turn_on=" + _("mac" + id).innerHTML,
        processData: false,
        success: function(data) {
            //
        }
    });
}

function get_name_from_device(device_name) {
    var first = device_name.split(' (');
    return first[0];
}

function get_ip_from_device(device_name) {
    console.log(device_name);
    var first = device_name.split('(');
    var second = first[1].split(')');
    return second[0];
}

function fall_asleep(id) {
    $.ajax({
        xhr: function() {
            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function() {
                if (xhr.readyState == 2) {
                    xhr.responseType = "text";
                }
            };
            return xhr;
        },
        url: window.location.pathname + "/~sleep",
        type: "PATCH",
        data: "turn_off=" + get_name_from_device(_("device_name" + id).innerHTML) + "&ip=" + get_ip_from_device(_("device_name" + id).innerHTML),
        processData: false,
        success: function(data) {
            //
        }
    });
}

var update_on_states = async function() {
    while (true) {
        $.ajax({
            xhr: function() {
                var xhr = new XMLHttpRequest();
                xhr.onreadystatechange = function() {
                    if (xhr.readyState == 2) {
                        xhr.responseType = "text";
                    }
                };
                return xhr;
            },
            url: window.location.pathname + "/~up",
            type: "GET",
            processData: false,
            success: function(data) {
                for (var i = 0; i < data.length; i++) {
                    on_state_id = "on_state" + (i + 1);
                    _(on_state_id).disabled = false;
                    if (data[i] == "1" && !_(on_state_id).checked)
                        _(on_state_id).click();
                    if (data[i] == "0" && _(on_state_id).checked)
                        _(on_state_id).click();
                    _(on_state_id).disabled = true;
                }
            }
        });
        await sleep(1000);
    }
}

update_on_states().then();