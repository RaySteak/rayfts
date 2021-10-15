document.title = window.location.host + window.location.pathname;

var perfEntries = performance.getEntriesByType("navigation");

if (perfEntries[0].type === "back_forward") {
    location.reload(true);
}

async function downloadZip(id) {
    try {
        _("hidden_frame" + id).remove(); //remove any frame created by previous calls
    } catch (error) {}
    var filename_td = _("row" + id).firstChild.nextSibling;
    var folder_name = filename_td.firstChild.title;
    folder_name += "/~archive";
    var url = window.location.pathname + folder_name;
    var iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    iframe.setAttribute("style", "position: absolute;width:0;height:0;border:0;");
    iframe.setAttribute("id", "hidden_frame" + id);
    var a = iframe.contentDocument.createElement("a");
    a.href = url;
    a.click();
}

function check() {
    var folder_name = _("folder_name").value;
    var not_allowed = ["&", "/", "%", "~", "..", "="];
    var create_button = _("create_folder_button");
    var error_message = _("error");
    if (folder_name.length > 255) {
        error_message.innerHTML = "Names must not be longer than 255 characters";
        create_button.disabled = true;
        return;
    }
    if (not_allowed.some(letter => folder_name.includes(letter))) {
        error_message.innerHTML = "Names must not contain the following: " + not_allowed.join(" ");
        create_button.disabled = true;
        return;
    }
    error_message.innerHTML = "";
    create_button.disabled = false;

}

async function done_uploading() {
    _("uploaded_text").innerHTML = "File uploaded, refreshing in 3";
    await sleep(1000);
    _("uploaded_text").innerHTML = "File uploaded, refreshing in 2";
    await sleep(1000);
    _("uploaded_text").innerHTML = "File uploaded, refreshing in 1";
    await sleep(1000);
    location.reload();
}

function uploadFile() {
    var fd = new FormData();
    fd.append('file', _("file1").files[0]);
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
        url: "#",
        type: "POST",
        data: fd,
        processData: false,
        contentType: false,
        success: function(result) {
            done_uploading().then();
        }
    });
}

function cancel_zip(id) {
    _("hidden_frame" + id).remove();
}

async function zipCheck(id) {
    var filename_td = _("row" + id).firstChild.nextSibling;
    console.log(filename_td.title);
    var button = filename_td.firstChild.nextSibling; // !!! this will change if structure of table changes !!!
    var cancel_button = button.cloneNode(true);
    cancel_button.setAttribute("style", "background-image: url('/images/cancel.png')"); //TODO: change image from node to attribute
    cancel_button.setAttribute("onclick", "cancel_zip(" + id + ")");
    filename_td.replaceChild(cancel_button, button); // disable button
    var td = document.createElement('td');
    var canvas = document.createElement('canvas');
    canvas.className = "inline_canvas";
    td.appendChild(canvas);
    _("row" + id).appendChild(td);
    var ctx = canvas.getContext('2d');
    var size = 20,
        linewidth = 4;
    canvas.width = canvas.height = size;
    var folder_name = filename_td.firstChild.title; // this as well
    var nr_fails = 0;
    var drawCircle = function(percentage) {
        ctx.clearRect(0, 0, size, size);
        ctx.beginPath();
        ctx.strokeStyle = '#efefef';
        ctx.arc(size / 2, size / 2, (size - linewidth) / 2, 0, 2 * Math.PI, false);
        ctx.stroke();
        ctx.beginPath();
        ctx.strokeStyle = 'rgb(255, 0, 0)';
        ctx.arc(size / 2, size / 2, (size - linewidth) / 2, -Math.PI / 2, 2 * Math.PI * percentage - Math.PI / 2, false);
        ctx.stroke();
    }
    while (true) {
        $.ajax({
            xhr: function() {
                var xhr = new XMLHttpRequest();
                xhr.onreadystatechange = function() {
                    if (xhr.readyState == 2) {
                        xhr.responseType = "text";
                        if (xhr.status != "200")
                            nr_fails++;
                    }
                };
                return xhr;
            },
            url: window.location.pathname + folder_name + "/~check",
            type: "GET",
            processData: false,
            success: function(data) {
                const sizes = data.split(" ");
                var cur_size = parseInt(sizes[0]);
                var total_size = parseInt(sizes[1]);
                percentage = cur_size / total_size;
                requestAnimationFrame(function() { drawCircle(percentage) });
            }
        });
        if (nr_fails >= 3)
            break;
        await sleep(200);
    }
    if (_("hidden_frame" + id) != null) {
        percentage = 1;
        requestAnimationFrame(drawCircle);
        await sleep(200);
    }
    _("row" + id).removeChild(td);
    filename_td.replaceChild(button, cancel_button); // add button back
    // wait a bit for the download to start before removing frame
    _("hidden_frame" + id).remove();
}


async function download_check_zip(id) {
    downloadZip(id).then();
    zipCheck(id);
}

window.onload = function() {
    var used = Number(_("used_space").innerHTML);
    var available = Number(_('free_space').innerHTML);
    _("free_space").remove();
    _("used_space").remove();
    var el = _('available_space_graph');
    var options = {
        percent: used / available * 100,
        size: el.getAttribute('data-size') || 220,
        lineWidth: el.getAttribute('data-line') || 15,
        rotate: el.getAttribute('data-rotate') || 0
    }
    var canvas = document.createElement('canvas');
    el.appendChild(document.createElement('br'));
    el.appendChild(document.createElement('br'));
    el.appendChild(document.createElement('br'));
    var span = document.createElement('span');
    span.textContent = Math.round((used / Math.pow(2, 30)) * 100) / 100 + " / " + Math.round(available / Math.pow(2, 30));
    if (typeof(G_vmlCanvasManager) !== 'undefined') {
        G_vmlCanvasManager.initElement(canvas);
    }
    var ctx = canvas.getContext('2d');
    canvas.width = canvas.height = options.size;
    el.appendChild(span);
    var span2 = document.createElement('span');
    span2.textContent = "GB used";
    el.appendChild(span2);
    el.appendChild(canvas);
    ctx.translate(options.size / 2, options.size / 2);
    ctx.rotate((-1 / 2 + options.rotate / 180) * Math.PI);
    var radius = (options.size - options.lineWidth) / 2;
    var drawDoubleCircle = function(bg_color, lineWidth, bg_percent, fg_percent, current) {
        ctx.clearRect(-options.size / 2, -options.size / 2, options.size, options.size);
        ctx.beginPath();
        ctx.arc(0, 0, radius, 0, Math.PI * 2 * current / bg_percent, false);
        ctx.strokeStyle = bg_color;
        ctx.lineCap = 'round';
        ctx.lineWidth = lineWidth;
        ctx.stroke();
        var current_fill = current / bg_percent * fg_percent / 100;
        ctx.strokeStyle = "rgb(" + (current_fill * 255) + "," + ((Math.max(1 - current_fill, 0.1)) * 255) + "," + 0 + ")";
        ctx.beginPath();
        ctx.arc(0, 0, radius, 0, Math.PI * 2 * current / bg_percent * fg_percent / 100, false);
        ctx.stroke();
        if (current < bg_percent) {
            requestAnimationFrame(function() {
                drawDoubleCircle(bg_color, lineWidth, bg_percent, fg_percent, current + 1);
            });
        }
    };
    drawDoubleCircle('#efefef', options.lineWidth, 100, options.percent, 0);
}

// abort so it dose not look stuck
window.onunload = () => {
    writableStream.abort()
        // also possible to call abort on the writer you got from `getWriter()`
    writer.abort()
}

window.onbeforeunload = evt => {
    if (!done) {
        evt.returnValue = `Are you sure you want to leave?`;
    }
}