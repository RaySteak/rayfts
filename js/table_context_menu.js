var ctxMenu = _("ctxMenu");
var ctxRenameForm = _("ctxRenameForm");
var currentContextedFile;

ctxMenu.addEventListener("mouseleave", function(Event) {
    ctxMenu.style.display = "";
    ctxMenu.style.left = "";
    ctxMenu.style.top = "";
}, false);

_("ctxRename").addEventListener("click", function(event) {
    ctxRenameForm.style.display = "block";
    ctxRenameForm.style.left = (event.pageX - 10) + "px";
    ctxRenameForm.style.top = (event.pageY - 10) + "px";
    _("ctxRenameForm").action = "./" + currentContextedFile + "/" + _("ctxRenameForm").getAttribute("action");
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


for (id = 0;; id++) {
    var row = _("row" + id);
    if (!row)
        break;

    row.addEventListener("contextmenu", function(event) {
        event.preventDefault();
        currentContextedFile = event.currentTarget.firstChild.nextSibling.firstChild.title;
        ctxMenu.style.display = "block";
        ctxMenu.style.left = (event.pageX - 10) + "px";
        ctxMenu.style.top = (event.pageY - 10) + "px";
    }, false);

}

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
        data: "new_name=laba",
        processData: false,
        contentType: false,
        success: function(result) {
            done_uploading().then();
        }
    });
}