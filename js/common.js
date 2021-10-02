function sleep(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function _(el) {
    return document.getElementById(el);
}