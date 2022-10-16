"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
require("chartjs-adapter-luxon");
const chart_js_1 = require("chart.js");
const chartjs_plugin_streaming_1 = __importDefault(require("chartjs-plugin-streaming"));
chart_js_1.Chart.register(chartjs_plugin_streaming_1.default);
chart_js_1.Chart.defaults.set('plugins.streaming', {
    duration: 20000
});
// Write TypeScript code!
const ctx = document.getElementById('myChart');
/* const myChart = */ new chart_js_1.Chart(ctx, {
    type: 'line',
    data: {
        labels: ['time'],
        datasets: [
            {
                label: 'x',
                data: [],
                backgroundColor: ['red']
            }
        ]
    },
    options: {
        scales: {
            x: {
                type: 'realtime',
                realtime: {
                    onRefresh: function (chart) {
                        chart.data.datasets.forEach(function (dataset) {
                            dataset.data.push({
                                x: Date.now(),
                                y: Math.random()
                            });
                        });
                    }
                }
            }
        }
    }
});
// console.log(myChart.generateLegend());
