var clone_tr = _("ref_tr").cloneNode(true);
var tbody = _("tbody");
_("ref_tr").remove();

function add_device(device_name, mac) {
    tbody.appendChild(clone_tr.cloneNode(true));
    _("device_name").innerHTML = device_name;
    _("mac").innerHTML = mac;
    _("device_name").removeAttribute("id");
    _("mac").removeAttribute("id");
}