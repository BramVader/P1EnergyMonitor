import 'chartjs-adapter-luxon';

import { Chart } from 'chart.js';
import ChartStreaming from 'chartjs-plugin-streaming';

Chart.register(ChartStreaming);

Chart.defaults.set('plugins.streaming', {
    duration: 20000
});

// Write TypeScript code!
const ctx = document.getElementById('myChart') as HTMLCanvasElement;
/* const myChart = */ new Chart(ctx, {
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
