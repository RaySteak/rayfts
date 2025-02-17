function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function _(el) {
    return document.getElementById(el);
}

function normalizeDirUrl(url) {
    if (url.endsWith("/")) {
        return url;
    }
    return url + "/";
}

function isImage(filename) {
    return filename.match(/\.(jpeg|jpg|gif|png)$/) != null;
}