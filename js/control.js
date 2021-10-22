_("on_state").disabled = true;
var awaken_button = _("awaken").cloneNode(true);
var fall_asleep_button = _("fall_asleep").cloneNode(true);
_("fall_asleep").remove();
var clone_tr = _("ref_tr").cloneNode(true);
var tbody = _("tbody");
var count = 0;
_("ref_tr").remove();

var types = [];

function add_device(device_name, mac, type) {
    tbody.appendChild(clone_tr.cloneNode(true));
    _("device_name").innerHTML = device_name;
    _("mac").innerHTML = mac;
    _("device_name").id = "device_name" + (++count);
    _("mac").id = "mac" + count;
    _("on_state").id = "on_state" + count;
    _("awaken").setAttribute("onClick", "javascript: awaken(" + count + ")");
    _("awaken").id = "awaken" + count;
    types[count] = type;
}

async function swap_state(id) {
    on_state_id = "on_state" + id;
    _(on_state_id).disabled = false;
    if (!_(on_state_id).checked) {
        var new_off = fall_asleep_button.cloneNode(true);
        new_off.id = "fall_asleep" + id;
        new_off.setAttribute("onClick", "javascript: fall_asleep(" + id + ")");
        _("awaken" + id).replaceWith(new_off);
        _(on_state_id).checked = true;
    } else {
        var new_on = awaken_button.cloneNode(true);
        new_on.id = "awaken" + id;
        new_on.setAttribute("onClick", "javascript: awaken(" + id + ")");
        _("fall_asleep" + id).replaceWith(new_on);
        _(on_state_id).checked = false;
    }
    //_(on_state_id).click();
    _(on_state_id).disabled = true;
}

var update_on_states = async function() {
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
                var id = i + 1;
                on_state_id = "on_state" + id;
                if (data[i] == "1" && !_(on_state_id).checked)
                    swap_state(id);
                else if (data[i] == "0" && _(on_state_id).checked)
                    swap_state(id);
            }
        }
    });
}

async function awaken(id) {
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
        data: "turn_on=" + _("mac" + id).innerHTML + "&ip=" + get_ip_from_device(_("device_name" + id).innerHTML) + "&type=" + types[id],
        processData: false,
        success: function(data) {
            //
        }
    });
    await sleep(100);
    update_on_states();
}

function get_name_from_device(device_name) {
    var first = device_name.split(' (');
    return first[0];
}

function get_ip_from_device(device_name) {
    var first = device_name.split('(');
    var second = first[1].split(')');
    return second[0];
}

async function fall_asleep(id) {
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
        data: "ip=" + get_ip_from_device(_("device_name" + id).innerHTML) + "&type=" + types[id],
        processData: false,
        success: function(data) {
            //
        }
    });
    await sleep(100);
    update_on_states();
}

var continually_update_on_states = async function() {
    while (true) {
        update_on_states();
        await sleep(1000);
    }
}

continually_update_on_states();