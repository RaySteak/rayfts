// _("on_state").disabled = true;
// var awaken_button = _("awaken").cloneNode(true);
// var fall_asleep_button = _("fall_asleep").cloneNode(true);
// _("fall_asleep").remove();
var clone_tr = _("ref_tr").cloneNode(true);
var clone_new_row = _("ref_new_row").cloneNode(true);
clone_tr.querySelector("#remove_row_button").addEventListener("click", function(event) {
    if (!event.target.id.startsWith("remove_row_button"))
        return;
     event.target.closest("#ref_new_row").remove();
     _("add_row_button").style.display = "inline-flex";
});
var count = 0;
_("ref_tr").remove();
_("ref_new_row").remove();

_("add_row_button").addEventListener("click", function() {
    if (_("mail_list").querySelector("#ref_new_row"))
        return;
    _("mail_list").appendChild(clone_new_row.cloneNode(true));
    _("add_row_button").style.display = "none";
});

document.addEventListener("click", function(event) {
    const remove_button = event.target.querySelector("[id^='remove_row_button']");
    if (!remove_button)
        return;

    const new_row = remove_button.closest("#ref_new_row");
    if (!new_row)
        return;

    new_row.remove();
    _("add_row_button").style.display = "inline-flex";
});

var types = [];

function populate_mail_list() {
    fetch("/mail/list").then(response => response.text()).then(data => {
        let lines = data.trim().split("\n");
        for (let line of lines) {
            let fields = line.split(",");
            if (fields.length < 5)
                continue;
            let sender_address = fields[0], sender_name = fields[1], recipient_address = fields[2], recipient_name = fields[3], mail_server = fields[4];
            let mail_item = clone_tr.cloneNode(true);
            let sender_address_elem = mail_item.querySelector("#table_sender_address");
            sender_address_elem.innerHTML = sender_address;
            let sender_name_elem = mail_item.querySelector("#table_sender_name");
            sender_name_elem.innerHTML = sender_name;
            let recipient_address_elem = mail_item.querySelector("#table_recipient_address");
            recipient_address_elem.innerHTML = recipient_address;
            let recipient_name_elem = mail_item.querySelector("#table_recipient_name");
            recipient_name_elem.innerHTML = recipient_name;
            let mail_server_elem = mail_item.querySelector("#table_mail_server");
            mail_server_elem.innerHTML = mail_server;
            mail_item.querySelector("#remove_row_button").id = "remove_row_button_" + count++;
            _("mail_list").appendChild(mail_item);
        }
    })
}

function clear_mail_list() {
    let mail_list = _("mail_list");
    let items = mail_list.querySelectorAll("#ref_tr");
    for (let item of items) {
        item.remove();
    }
}

function refresh_mail_list() {
    clear_mail_list();
    populate_mail_list();
}

// Override submit so we can wait for the server to update the mail list before refreshing it
_("mail_entry_form").addEventListener("submit", async function(event) {
    event.preventDefault();

    const response = await fetch("/mail", {
        method: "POST",
        // headers: {
        //     "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8"
        // },
        body: new URLSearchParams(new FormData(this))
    });

    if (response.ok) {
        refresh_mail_list();
    } else {
        alert("Failed to add mail item");
    }
});

populate_mail_list();