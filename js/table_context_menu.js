var ctxMenu = _("ctxMenu");
var ctxMenuShort = _("ctxMenuShort");
var ctxRenameForm = _("ctxRenameForm");
var currentContextedFile;
var currentContextedRow;

function setMenuWidths(menu) {
    if (menu.firstElementChild == null)
        return;

    let maxWidth = 0;

    for (let cur = menu.firstElementChild; cur != null; cur = cur.nextElementSibling) {
        let width = cur.offsetWidth;
        maxWidth = width > maxWidth ? width : maxWidth;
        setMenuWidths(cur);
    }

    for (let cur = menu.firstElementChild; cur; cur = cur.nextElementSibling) {
        cur.style.width = maxWidth + "px";
    }
}

setMenuWidths(ctxMenu);
setMenuWidths(ctxMenuShort);

// Menu utils
function hideMenu(menu, type) {
    return function(event) {
        menu.style.display = "";
        menu.style.visibility = "";
        menu.style.left = "";
        menu.style.top = "";
        if (type == "normal") {
            currentContextedRow.firstElementChild.nextElementSibling.nextElementSibling.nextElementSibling.firstElementChild.checked = false;
        }
    }
}

function showMenu(menu, type) {
    return function(event) {
        if (type == "normal") {
            event.currentTarget.lastElementChild.firstElementChild.checked = true;
        } else if (type == "short") {
            for (startingElement = event.target; startingElement && startingElement.id.startsWith("row") == false; startingElement = startingElement.parentElement)
                ;
            if (startingElement)
                return;
        }
        event.preventDefault();
        currentContextedFile = event.currentTarget.firstChild.nextSibling.firstChild.title;
        currentContextedRow = event.currentTarget;
        menu.style.display = "block";
        menu.style.visibility = "visible";
        menu.style.left = (event.pageX - 10) + "px";
        menu.style.top = (event.pageY - 10) + "px";
    }
}

// ctxMenu
ctxMenu.addEventListener("mouseleave", hideMenu(ctxMenu, "normal"), false);

for (id = 0;; id++) {
    var row = _("row" + id);
    if (!row)
        break;

    row.addEventListener("contextmenu", showMenu(ctxMenu, "normal"), false);
}

// ctxMenuShort
ctxMenuShort.addEventListener("mouseleave", hideMenu(ctxMenuShort, "short"), false);

// TODO: maybe refine when the short many actually shows and where to click to make it show
_("table").addEventListener("contextmenu", showMenu(ctxMenuShort, "short"), false);

// Rename
_("ctxRename").addEventListener("click", function(event) {
    ctxRenameForm.style.display = "block";
    ctxRenameForm.style.left = (event.pageX - 10) + "px";
    ctxRenameForm.style.top = (event.pageY - 10) + "px";
    _("ctxRenameForm").action = "./" + currentContextedFile + "/" + _("ctxRenameForm").getAttribute("action");
    _("ctxRenameForm").firstElementChild.value = currentContextedFile;
}, false);

ctxRenameForm.addEventListener("click", function(event) { event.stopPropagation() }, false);

document.addEventListener("click", function(event) {
    if (ctxMenu.style.display == "") {
        ctxRenameForm.style.display = "";
        ctxRenameForm.style.left = "";
        ctxRenameForm.style.top = "";
    }
    ctxMenu.style.display = "";
    ctxMenu.style.left = "";
    ctxMenu.style.top = "";
}, false);

// Fast download
_("ctxEncDownload").addEventListener("click", function (event) {
    var a = document.createElement("a");
    // TODO: !!!!!!when changing table structure this will also change
    type = currentContextedRow.firstElementChild.firstElementChild.src.split("/").at(-1).split(".")[0];
    //
    if (type == "file") {
        a.href = window.location.pathname + currentContextedFile + "/~encDownload";
        a.click();
        a.remove();
    } else {
        a.href = window.location.pathname + currentContextedFile + "/~encArchive";
        download_check_zip(parseInt(currentContextedRow.id.substring(3)));
    }
}, false);

// Cut
_("ctxCut").addEventListener("click", function(event) {
    post_data = "";
    for (id = 0;; id++) {
        row = _("row" + id) 
        if (!row)
            break;
        checkbox = row.lastElementChild.firstElementChild
        if (checkbox.checked)
            post_data += checkbox.name + "&";
        checkbox.checked = false;
    }

    post_data = post_data.slice(0, -1);

    $.ajax({
        url: window.location.pathname + "~cut",
        type: "POST",
        data: post_data,
        success: function(result) {
        }
    });
}, false);

// Paste
pasteEvent = function(event) {
    $.ajax({
        url: window.location.pathname + "~paste",
        type: "POST",
        success: function(result) {
            location.reload();
        }
    });
}


_("ctxPaste").addEventListener("click", pasteEvent, false);
_("ctxPasteShort").addEventListener("click", pasteEvent, false);

// Old test renaming function
function renameFile(filename) {
    $.ajax({
        xhr: function() {
            var xhr = new window.XMLHttpRequest();

            xhr.upload.addEventListener("progress", function(evt) {
                if (evt.lengthComputable) {
                    var percentComplete = evt.loaded / evt.total;
                    percentComplete = parseInt(percentComplete * 100);
                    console.log(percentComplete);
                    _("progressBar").value = percentComplete;
                    _("uploaded_text").innerHTML = percentComplete + "%";
                    if (percentComplete === 100) {}

                }
            }, false);

            return xhr;
        },
        url: window.location.pathname + filename + "/~rename",
        type: "POST",
        data: "new_name=test_name.txt",
        processData: false,
        contentType: false,
        success: function(result) {
            done_uploading().then();
        }
    });
}